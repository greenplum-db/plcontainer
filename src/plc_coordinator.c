/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"
#include <unistd.h>
#include <sys/inotify.h>

#include "access/tupdesc.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_database.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/extension.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/idle_resource_cleaner.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/formatting.h"
#include "utils/memutils.h"
#include "utils/numeric.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "storage/shm_toc.h"

#include "plc/plc_docker_api.h"
#include "plc/plc_configuration.h"
#include "plc/plc_coordinator.h"
#include "common/comm_shm.h"
#include "common/messages/messages.h"
#include "interface.h"

PG_MODULE_MAGIC;
// PROTOTYPE:
extern void _PG_init(void);
extern void plc_coordinator_main(Datum datum);
extern void plc_coordinator_aux_main(Datum datum);
extern int PlcDocker_create(runtimeConfEntry *conf, char **name, char *uds_dir, int qe_pid, int session_id, int ccnt,int uid, int gid,int procid, char* dbname, char *ownername);
extern int PlcDocker_start(char *id, char *msg);
extern int PlcDocker_delete(const char **ids, int length, char* msg);
extern int PlcDocker_stat(const char** ids, int length, int64_t *mem_usage);
// END OF PROTOTYPES.

static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

CoordinatorStruct *coordinator_shm;
#define RECEIVE_BUF_SIZE 2048
#define TIMEOUT_SEC 3
#define MAX_START_RETRY 5
#define INSPECT_DOCKER_AT_ROUNT 5
/* meesage queue */
shm_mq_handle *message_queue_handle;
ShmqBufferStatus *message_queue_status;
CoordinatorConstraint *coordinator_docker_constraint;
/* debug use only */
bool plcontainer_stand_alone_mode = true;
int plc_max_docker_creating_num = 3;
char *plcontainer_stand_alone_server_path;
static int send_message(QeRequest *request);
static int receive_message();
static int handle_request(QeRequest *req);
static int start_stand_alone_process(const char* uds_address);
static void shm_message_queue_receiver_init(dsm_segment *seg);
static dsm_handle shm_message_queue_sender_init();
static int update_containers_status(bool inspect);

HTAB *container_status_table;

static void
plc_coordinator_shmem_startup(void)
{
    bool found;
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();
    coordinator_shm = ShmemInitStruct(CO_SHM_KEY, MAXALIGN(sizeof(CoordinatorStruct)), &found);
    Assert(!found);
    coordinator_shm->state = CO_STATE_UNINITIALIZED;
}

static void
request_shmem_(void)
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(CoordinatorStruct)));

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = plc_coordinator_shmem_startup;
}

HTAB*
init_runtime_info_table() {
	HASHCTL hash_ctl;
	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(ContainerKey);
	hash_ctl.entrysize = sizeof(ContainerEntry);
	hash_ctl.hcxt = CurrentMemoryContext;
	hash_ctl.hash = string_hash;
	return hash_create("container info hash",
								8,
								&hash_ctl,
								HASH_ELEM | HASH_FUNCTION);
}

static void
plc_coordinator_sigterm(pg_attribute_unused() SIGNAL_ARGS)
{
    int save_errno = errno;
    got_sigterm = true;
    SetLatch(&MyProc->procLatch);
    errno = save_errno;
}

static void
plc_coordinator_sighup(pg_attribute_unused() SIGNAL_ARGS)
{
    int save_errno = errno;

    got_sighup = true;
    SetLatch(&MyProc->procLatch);
    errno = save_errno;
}

/**
 * Initialize coordinator:
 * 1. Create socket to listen request from QE
 * 2. Create coordinator aux process to do some hard work,
 *     like inspect docker
 */
static int
plc_initialize_coordinator(dsm_handle seg)
{
	BackgroundWorker auxWorker;

    char address[1024] = {0};
    snprintf(address, sizeof(address), "/tmp/.plcoordinator.%ld.unix.sock", (long)getpid());
    coordinator_shm->protocol = CO_PROTO_UNIX;
	coordinator_docker_constraint = (CoordinatorConstraint*) top_palloc(sizeof(CoordinatorConstraint));
	memset(coordinator_docker_constraint, 0, sizeof(CoordinatorConstraint));
    strcpy(coordinator_shm->address, address);

	if (plc_refresh_container_config(false) != 0) {
		if (runtime_conf_table == NULL) {
			/* can't load runtime configuration */
			elog(WARNING, "PL/container: can't load runtime configuration");
		} else {
			elog(WARNING, "PL/container: there is no runtime configuration");
		}
	}

	memset(&auxWorker, 0, sizeof(auxWorker));
	auxWorker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	auxWorker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	auxWorker.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
	auxWorker.bgw_main = plc_coordinator_aux_main;
	auxWorker.bgw_main_arg = UInt32GetDatum(seg);

	auxWorker.bgw_notify_pid = 0;
	snprintf(auxWorker.bgw_name, sizeof(auxWorker.bgw_name), "plcoordinator_aux");
	RegisterDynamicBackgroundWorker(&auxWorker, NULL);
	return 0;
}
void handle_config_file_events(int fd, int wd)
{
	/* Some systems cannot read integer variables if they are not
		properly aligned. On other systems, incorrect alignment may
		decrease performance. Hence, the buffer used for reading from
		the inotify file descriptor should have the same alignment as
		struct inotify_event. */

	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	ssize_t len;
	char *ptr;

	/* Loop while events can be read from inotify file descriptor. */

	for (;;) {

		/* Read some events. */

		len = read(fd, buf, sizeof(buf));
		if (len == -1 && errno != EAGAIN) {
			elog(ERROR, "failed to read config file.");
			return;
		}

		/* If the nonblocking read() found no events to read, then
			it returns -1 with errno set to EAGAIN. In that case,
			we exit the loop. */

		if (len <= 0)
			break;

		/* Loop over all events in the buffer */

		for (ptr = buf; ptr < buf + len;
				ptr += sizeof(struct inotify_event) + event->len) {

			event = (const struct inotify_event *) ptr;
			if (event->mask & IN_CLOSE_WRITE) {
				elog(LOG, "config file close write");
				if (plc_refresh_container_config(false) != 0) {
					if (runtime_conf_table == NULL) {
						/* can't load runtime configuration */
						elog(WARNING, "PL/container: can't refresh runtime configuration");
					} else {
						elog(WARNING, "PL/container: no runtime configuration changed");
					}
				} else {
					elog(LOG, "PL/container: refresh runtime configuration");
				}
			}
		}
	}
}

void
plc_coordinator_main(Datum datum)
{
    int rc;
	dsm_handle seg;
	int configfd, configwd;
	
    (void)datum;
    pqsignal(SIGTERM, plc_coordinator_sigterm);
    pqsignal(SIGHUP, plc_coordinator_sighup);
	seg = shm_message_queue_sender_init();
	configfd = inotify_init1(IN_NONBLOCK);
	if (configfd < 0) {
		elog(ERROR, "failed to do inotify_init");
	}
	char* configfile = get_config_filename();
	elog(LOG, "config file is %s", configfile);
	configwd = inotify_add_watch(configfd, configfile, IN_OPEN | IN_CLOSE | IN_MODIFY);
	
    plc_initialize_coordinator(seg);
    BackgroundWorkerUnblockSignals();

    coordinator_shm->state = CO_STATE_READY;
    elog(INFO, "plcoordinator is going to enter main loop");
	if (plcontainer_stand_alone_mode) {
		container_status_table = init_runtime_info_table();
		if (container_status_table == NULL) {
			elog(ERROR, "Failed to init container status hash table");
		}
	}

    PLCoordinatorServer *server = start_server(coordinator_shm->address);
    plc_elog(LOG, "server is started on address:%s", server->address);

    while (!got_sigterm) {
        ResetLatch(&MyProc->procLatch);
        if (got_sighup) {
            got_sighup = false;
        }
        rc = WaitLatch(&MyProc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, 0);
        if (rc & WL_POSTMASTER_DEATH)
            break;
        handle_config_file_events(configfd, configwd);
        if (process_request(server, TIMEOUT_SEC) < 0) {
            plc_elog(ERROR, "server process request error.");
        }

        if (plcontainer_stand_alone_mode) {
            update_containers_status(false);
        }
		
    }

    if (coordinator_shm->protocol != CO_PROTO_TCP)
        unlink(coordinator_shm->address);
    proc_exit(0);
}

void
plc_coordinator_aux_main(Datum datum)
{
    int rc;
	dsm_segment *seg;
	if (!plcontainer_stand_alone_mode) {
		container_status_table = init_runtime_info_table();
		if (container_status_table == NULL) {
			elog(ERROR, "Failed to init container status hash table");
		}
	}
    pqsignal(SIGTERM, plc_coordinator_sigterm);
    pqsignal(SIGHUP, SIG_IGN);
	/* TODO: error process */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "plcontainer monitor");
	seg = dsm_attach(DatumGetUInt32(datum));
	shm_message_queue_receiver_init(seg);
    BackgroundWorkerUnblockSignals();
	bool inspect = false;
	int round_count = 0;
    while(!got_sigterm) {
        ResetLatch(&MyProc->procLatch);
        rc = WaitLatch(&MyProc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, 0);
        if (rc & WL_POSTMASTER_DEATH)
            break;
        receive_message();
        if (!plcontainer_stand_alone_mode) {
			if (round_count % INSPECT_DOCKER_AT_ROUNT == 0)
			{
				inspect = true;
				round_count = 0;
			} else {
				inspect = false;
			}
            update_containers_status(inspect);
        }
        sleep(2);
		round_count++;
    }

    proc_exit(0);
}

void
_PG_init(void)
{
    BackgroundWorker worker;

    request_shmem_();
    memset(&worker, 0, sizeof(BackgroundWorker));

    /* coordinator.so must be in shared_preload_libraries to init SHM. */
    if (!process_shared_preload_libraries_in_progress)
        ereport(ERROR, (errmsg("plc_coordinator.so not in shared_preload_libraries.")));

	/* Register GUC vaule for stand alone mode */

	DefineCustomBoolVariable("plcontainer.stand_alone_mode",
							 "Enable plcontainer server side stand alone mode",
							 NULL,
							 &plcontainer_stand_alone_mode,
							 false,
							 PGC_SIGHUP,
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomStringVariable("plcontainer.server_path",
							   "Set the path of the stand alone server",
							   NULL,
							   &plcontainer_stand_alone_server_path,
							   "none",
							   PGC_SIGHUP,
							   0,
							   NULL,
							   NULL,
							   NULL);
	DefineCustomIntVariable("plcontainer.max_creating_docker_num",
							"The max number of creating dockers at the same time",
							NULL,
							&plc_max_docker_creating_num,
							3, 1, 10,
							PGC_SIGHUP,
							0,
							NULL,
							NULL,
							NULL);

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
    worker.bgw_main = plc_coordinator_main;

    worker.bgw_notify_pid = 0;

    snprintf(worker.bgw_name, BGW_MAXLEN, "[plcontainer] - coordinator");

    RegisterBackgroundWorker(&worker);
    elog(NOTICE, "init plc_coordinator %d done", (int)getpid());
}

static int store_container_info(ContainerKey *key, pid_t server_pid, char* container_id)
{
	ContainerEntry *entry = NULL;
	bool found = false;
	entry = (ContainerEntry *) hash_search(container_status_table, key, HASH_ENTER,
											&found);
	if (found) {
		if (entry->stand_alone_pid != 0) {
			elog(LOG, "previous server process exist %d", entry->stand_alone_pid);
		}
		if (entry->containerId[0] != '\0') {
			elog(LOG, "key %d previous container Id exist %s, will delete it", key->qe_pid, entry->containerId);
			plc_docker_delete_container(entry->containerId);
		}
	}
	entry->stand_alone_pid = server_pid;
	entry->key.qe_pid = key->qe_pid;
	entry->key.conn = key->conn;
	snprintf(entry->containerId, sizeof(entry->containerId), "%s", container_id);
	elog(LOG, "put key %p %d value %s into hash table", &(entry->key), entry->key.qe_pid, entry->containerId);
	return 0;
}

static int clear_container_info(ContainerKey *key)
{
	ContainerEntry *entry = NULL;
	bool found = false;
	int res = 0;
	entry = (ContainerEntry *) hash_search(container_status_table, key, HASH_FIND,
											&found);
	if (!found || entry == NULL) {
		elog(LOG, "failed to find server info");
		return -1;
	}
	if (entry->stand_alone_pid != 0) {
		elog(LOG, "try to terminate server process %d", entry->stand_alone_pid);
		res = kill(entry->stand_alone_pid, SIGKILL);
		if (waitpid(entry->stand_alone_pid, NULL, 0) < 0) {
			elog(LOG, "failed terminate server process %d", entry->stand_alone_pid);
		}
		
		entry->stand_alone_pid = 0;
		return 0;
	}
	if (entry->containerId[0] != '\0') {
		res = plc_docker_delete_container(entry->containerId);
		snprintf(entry->containerId, sizeof(entry->containerId), "");
	}

	return res;
}

int create_container(runtimeConfEntry *runtime_entry, ContainerKey *key, char **docker_name, char *uds_dir, int dbid, char *ownername)
{
	int res = 0;
	SpinLockAcquire(&coordinator_docker_constraint->mutex);
	if (coordinator_docker_constraint->dockerCreating < plc_max_docker_creating_num) {
		coordinator_docker_constraint->dockerCreating++;
	} else {
		SpinLockRelease(&coordinator_docker_constraint->mutex);
		elog(WARNING, "Too many dockers are creating now, retry later");
		return -1;
	}
	SpinLockRelease(&coordinator_docker_constraint->mutex);

	res = PlcDocker_create(runtime_entry, docker_name, uds_dir, key->qe_pid, key->conn, key->ccnt, getuid(), getgid(),MyProcPid, dbid, ownername);

	if (res != 0) {
		elog(WARNING, "create container failed");
		SpinLockAcquire(&coordinator_docker_constraint->mutex);
		coordinator_docker_constraint->dockerCreating--;
		SpinLockRelease(&coordinator_docker_constraint->mutex);
		return -1;
	}
	QeRequest *request = (QeRequest*) palloc (sizeof(QeRequest));
	request->pid = key->qe_pid;
	request->conn = key->conn;
	request->ccnt = key->ccnt;
	request->requestType = CREATE_SERVER;
	request->server_pid = 0;
	snprintf(request->containerId, sizeof(request->containerId), "%s", *docker_name);
	res = send_message(request);
	if (res != 0)
	{
		elog(WARNING, "send start server message for %d--%s failure", key->qe_pid, *docker_name);
	} else {
		elog(LOG, "send start server message for %d--%s success", key->qe_pid, *docker_name);
	}
	SpinLockAcquire(&coordinator_docker_constraint->mutex);
	coordinator_docker_constraint->dockerCreating--;
	SpinLockRelease(&coordinator_docker_constraint->mutex);
	return res;
}

int run_container(char *docker_name)
{
	int res = 0;

	res = plc_docker_start_container(docker_name);
	if (res != 0) {
		elog(WARNING, "start container failed");
		return -1;
	}
	return 0;
}
int start_container(const char *runtimeid, pid_t qe_pid, int session_id, int ccnt, int dbid, const char *ownername, char **uds_address, char **container_id, char **log_msg)
{
	pid_t server_pid;
	int res;
	struct timeval create_start_time, create_end_time, start_end_time;
	*uds_address = (char*) palloc(DEFAULT_STRING_BUFFER_SIZE);
	*log_msg = (char*) palloc(MAX_LOG_LENGTH);
	ContainerKey key;
	key.conn = session_id;
	key.qe_pid = qe_pid;
	key.ccnt = ccnt;

	/* debug test only, we need to store the container info in coordinator */
	if (plcontainer_stand_alone_mode)
	{
		snprintf(*uds_address, DEFAULT_STRING_BUFFER_SIZE, "%s.%d.%d.%d.%d", DEBUG_UDS_PREFIX, qe_pid, session_id, ccnt, (int)getpid());
		server_pid = start_stand_alone_process(*uds_address);
		*container_id = (char *) palloc(DEFAULT_STRING_BUFFER_SIZE);
		snprintf(*container_id, DEFAULT_STRING_BUFFER_SIZE, "standalone_pid_%d", server_pid);
		store_container_info(&key, server_pid, NULL);
		*log_msg[0] = '\0';
		return 0;
	} else {
		runtimeConfEntry *runtime_entry = plc_get_runtime_configuration(runtimeid);
		if (runtime_entry == NULL) {
			elog(WARNING, "Cannot find runtime configuration %s", runtimeid);
			return -1;
		}
		char *uds_dir = palloc(DEFAULT_STRING_BUFFER_SIZE);
		*container_id = (char *) palloc(DEFAULT_STRING_BUFFER_SIZE);
		sprintf(uds_dir,  "%s.%d.%d.%d.%d", UDS_PREFIX, qe_pid, session_id, ccnt, (int)getpid());
		snprintf(*uds_address, DEFAULT_STRING_BUFFER_SIZE, "%s/%s", uds_dir, UDS_SHARED_FILE);
		int retry_count = 0;
		bool created = false;
		res = -1;
		gettimeofday(&create_start_time, NULL);
		char *msg = (char *) palloc(DEFAULT_STRING_BUFFER_SIZE);
		memset(msg, 0 ,DEFAULT_STRING_BUFFER_SIZE);
		while (retry_count < MAX_START_RETRY) {
			if (!created) {
				res = create_container(runtime_entry, &key, container_id, uds_dir,dbid, ownername);
				
				created = true;
				gettimeofday(&create_end_time, NULL);
				
			}

			res = PlcDocker_start(*container_id, msg);
			if (res == 0) {
				gettimeofday(&start_end_time, NULL);
				break;
			} else {
				elog(LOG, "failed to start %s: %s", *container_id, msg);
			}
			retry_count++;
			sleep(2);
		}
		pfree(msg);
		int create_cost_time_ms = 0;
		int start_cost_time_ms = 0;
		if (res == 0) {
			create_cost_time_ms = create_end_time.tv_sec * 1000 + create_end_time.tv_usec / 1000 
						- create_start_time.tv_sec * 1000 - create_start_time.tv_usec / 1000;
			start_cost_time_ms = start_end_time.tv_sec * 1000 + start_end_time.tv_usec / 1000 
						- create_end_time.tv_sec * 1000 - create_end_time.tv_usec / 1000;
			snprintf(*log_msg, MAX_LOG_LENGTH, "create cost: %d, start cost: %d, retry: %d", create_cost_time_ms, start_cost_time_ms, retry_count);
		}
		return res;
	}
}

int destroy_container(pid_t qe_pid, int session_id, int ccnt)
{
	/* if we are in stand alone mode kill the process in coordinator to avoid defunct */
	if (plcontainer_stand_alone_mode) {
		ContainerKey key;
		key.conn = session_id;
		key.qe_pid = qe_pid;
		key.ccnt = ccnt;
		clear_container_info(&key);
		return 0;
	} else {
		QeRequest *request;
		int res;
		request = (QeRequest*) palloc (sizeof(QeRequest));
		memset(request, 0, sizeof(QeRequest));
		request->pid = qe_pid;
		request->conn = session_id;
		request->ccnt = ccnt;
		request->requestType = DESTROY_SERVER;
		res = send_message(request);
		if (res != 0)
		{
			elog(WARNING, "send destroy message for qe %d failure", qe_pid);
			return -1;
		} else {
			elog(LOG, "send destroy message for qe %d success", qe_pid);
			return 0;
		}
	}
}

static int send_message(QeRequest *request)
{
	shm_mq_result res;

	/* Must wait if the message queue is full */
	res = shm_mq_send(message_queue_handle, sizeof(QeRequest), request, false);
	if (res != SHM_MQ_SUCCESS)
	{
		return -1;
	} else {
		return 0;
	}
}

static int receive_message()
{
	shm_mq_result res;
	Size nbytes;
	void *data;
	int result = 0;
	for (;;)
	{
		/* Receive does not need to be blocked */
		res = shm_mq_receive(message_queue_handle, &nbytes, &data, true);

		if (res == SHM_MQ_WOULD_BLOCK)
		{
			break;
		} else if (res == SHM_MQ_SUCCESS) {
			if (nbytes == sizeof(QeRequest))
			{
				QeRequest *request;

				request = (QeRequest*) data;
				elog(LOG, "PLC coordinator: receive message %d.%d --- %s", request->pid, request->conn, request->containerId);
				handle_request(request);
			}
		} else {
			elog(LOG, "PLC coordinator: mq res %d", res);
			result = -1;
			break;
		}
	}
	return result;
}
static int update_containers_status(bool inspect)
{
    int entry_num = 0;
    int res = 0;
    int i = 0;
    entry_num = hash_get_num_entries(container_status_table);
    if (entry_num == 0) {
        return 0;
    }
    HASH_SEQ_STATUS scan;
    ContainerEntry *container_entry = NULL;
    hash_seq_init(&scan, container_status_table);
	int entry_size = entry_num * sizeof(ContainerKey *);
    ContainerKey **entry_array = palloc(entry_size);
	memset(entry_array, 0, entry_size);
	int delete_ids_size = entry_num * sizeof(char*);
	char **delete_ids = palloc(delete_ids_size);
	memset(delete_ids, 0, delete_ids_size);
	while ((container_entry = (ContainerEntry *) hash_seq_search(&scan)) != NULL) {
        if (!plcontainer_stand_alone_mode) {
			/* check process first */
            res = kill(container_entry->key.qe_pid, 0);
            if (res != 0 ) {
                elog(LOG, "delete container %s of pid %d session id %d ccnt %d", container_entry->containerId, container_entry->key.qe_pid, container_entry->key.conn, container_entry->key.ccnt);
				entry_array[i] = &(container_entry->key);
				delete_ids[i] = &(container_entry->containerId);
				i++;
				continue;
            }
			
			if (inspect) {
				res = plc_docker_inspect_container(container_entry->containerId, &container_entry->status, PLC_INSPECT_STATUS);
				if (res < 0) {
					elog(LOG, "Failed to inspect container %s", container_entry->containerId);
					continue;
				}
				if (strcmp(container_entry->status, "exited") == 0) {
					//plc_docker_delete_container(container_entry->containerId);
					delete_ids[i] = &(container_entry->containerId);
					entry_array[i] = &(container_entry->key);
					i++;
				}
			}
        } else {
            if (kill(container_entry->key.qe_pid, 0) != 0) {
                destroy_container(container_entry->key.qe_pid, container_entry->key.conn, container_entry->key.ccnt);
                entry_array[i++] = &(container_entry->key);
            }
        }
	}
	if (i > 0 && !plcontainer_stand_alone_mode) {
		char *msg = (char *) palloc(DEFAULT_STRING_BUFFER_SIZE);
		memset(msg, 0 ,DEFAULT_STRING_BUFFER_SIZE);
		int res = PlcDocker_delete((const char**)delete_ids, i, msg);
		if (res < 0) {
			elog(LOG, "delete failed %s", msg);
		} else {
			elog(LOG, "success delete %d containers", i);
		}
		pfree(msg);
	}
	for (int j = 0; j < i; j++) {
		hash_search(container_status_table, entry_array[j], HASH_REMOVE, NULL);
	}
	pfree(entry_array);
	pfree(delete_ids);
    return 0;
}

static int handle_request(QeRequest *req)
{
	int res = 0;
	ContainerKey* key = palloc(sizeof(ContainerKey));
	key->conn = req->conn;
	key->qe_pid = req->pid;
	key->ccnt = req->ccnt;
	switch (req->requestType) {
		case CREATE_SERVER:
			store_container_info(key, 0, req->containerId);
			break;
		case DESTROY_SERVER:
			clear_container_info(key);
			break;
		default:
			break;
	}
	return res;
}

static int start_stand_alone_process(const char* uds_address)
{
	pid_t pid = 0;

	pid = fork();
	if (pid == 0)
		execl(plcontainer_stand_alone_server_path, plcontainer_stand_alone_server_path, "1", uds_address, NULL);
	else if (pid < 0)
		elog(WARNING, "Can not start server %s in debug mode", plcontainer_stand_alone_server_path);

	return pid;
}

static void shm_message_queue_receiver_init(dsm_segment *seg)
{
	shm_toc    *toc;
	shm_mq *message_queue;


	if (seg == NULL)
		elog(ERROR, "unable to map dynamic shared memory segment");

	toc = shm_toc_attach(PLC_COORDINATOR_MAGIC_NUMBER, dsm_segment_address(seg));
	if (toc == NULL)
		elog(ERROR, "bad magic number in dynamic shared memory segment");

	/* Using key 0 to connect global message queue status */
	message_queue_status = shm_toc_lookup(toc, 0);
	SpinLockAcquire(&message_queue_status->mutex);
	if (message_queue_status->state != CO_STATE_BUFFER_INITIALIZED) {
		SpinLockRelease(&message_queue_status->mutex);
		elog(ERROR, "message queue is not ready");
	}
	message_queue_status->state = CO_STATE_BUFFER_ATTACHED;
	SpinLockRelease(&message_queue_status->mutex);

	/* get the message queue with key 1 */
	message_queue = shm_toc_lookup(toc, 1);

	shm_mq_set_receiver(message_queue, MyProc);
	message_queue_handle = shm_mq_attach(message_queue, seg, NULL);

}

static dsm_handle shm_message_queue_sender_init()
{
	shm_toc_estimator e;
	Size segsize;
	dsm_segment *seg;
	shm_toc    *toc;
	Size buffersize;
	shm_mq *message_queue;

	/* Calculate how many memory that needed allocated for message queue
	 * In message queue, we have two keys, one key is for message queue status
	 * another key is for queue buffer
	 */
	shm_toc_initialize_estimator(&e);

	/* size of message queue status */
	shm_toc_estimate_chunk(&e, sizeof(ShmqBufferStatus));
	shm_toc_estimate_keys(&e, 1);

	/* size of queue buffer */
	buffersize = sizeof(QeRequest) * SHMQ_BUFFER_BLOCK_NUMBER;
	shm_toc_estimate_chunk(&e, buffersize);
	shm_toc_estimate_keys(&e, 1);

	segsize = shm_toc_estimate(&e);

	/* Create the shared memory segment and establish a table of contents. */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "plcontainer coordinator");
	seg = dsm_create(shm_toc_estimate(&e));
	toc = shm_toc_create(PLC_COORDINATOR_MAGIC_NUMBER, dsm_segment_address(seg),
						 segsize);

	/* Set up the message queue status region. */
	message_queue_status = shm_toc_allocate(toc, sizeof(ShmqBufferStatus));
	SpinLockInit(&message_queue_status->mutex);
	message_queue_status->state = CO_STATE_BUFFER_INITIALIZED;

	/* The first content with key id 0 */
	shm_toc_insert(toc, 0, message_queue_status);

	/* Create message queue with key id 1 */
	message_queue = shm_mq_create(shm_toc_allocate(toc, buffersize),
						   buffersize);
	shm_toc_insert(toc, 1, message_queue);

	/* Main process is sender */
	shm_mq_set_sender(message_queue, MyProc);
	message_queue_handle = shm_mq_attach(message_queue, seg, NULL);

	return dsm_segment_handle(seg);
}
