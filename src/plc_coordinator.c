/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"
#include <unistd.h>

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
#include "libpq/libpq-be.h"
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

#include "common/base_network.h"
#include "plc/plc_configuration.h"
#include "plc/plc_coordinator.h"
#include "common/comm_shm.h"
#include "common/comm_channel.h"
#include "common/messages/messages.h"

PG_MODULE_MAGIC;
// PROTOTYPE:
extern void _PG_init(void);
extern void plc_coordinator_main(Datum datum);
extern void plc_coordinator_aux_main(Datum datum);

// END OF PROTOTYPES.

static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

CoordinatorStruct *coordinator_shm;
plcConn *conn;
#define RECEIVE_BUF_SIZE 2048
#define TIMEOUT_SEC 3
/* meesage queue */
shm_mq_handle *message_queue_handle;
ShmqBufferStatus *message_queue_status;

/* debug use only */
bool plcontainer_stand_alone_mode = true;
char *plcontainer_stand_alone_server_path;


static int start_container(const char *runtimeid, pid_t qe_pid, int session_id, int ccnt, char **uds_address);
static int send_message(QeRequest *request);
static int receive_message();
static int start_stand_alone_process(const char* uds_address);
static void shm_message_queue_receiver_init(dsm_segment *seg);
static dsm_handle shm_message_queue_sender_init();


// HTAB *container_status_table;

static void
plc_coordinator_shmem_startup(void)
{
    bool found;
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();
    // TODO: do shared memory initialization
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
static void
plc_coordinator_sigterm(pg_attribute_unused() SIGNAL_ARGS)
{
    got_sigterm = true;
}

static void
plc_coordinator_sighup(pg_attribute_unused() SIGNAL_ARGS)
{
    got_sighup = true;
}

static int
plc_listen_socket()
{
    char address[500];
    int sock;
    snprintf(address, sizeof(address), "/tmp/.plcoordinator.%ld.unix.sock", (long)getpid());
    sock = plcListenServer("unix", address);
    if (sock < 0) {
        elog(ERROR, "initialize socket failed");
    }
    coordinator_shm->protocol = CO_PROTO_UNIX;
    strcpy(coordinator_shm->address, address);
    return sock;
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
	int sock;

	sock = plc_listen_socket();

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
	conn = (plcConn *) palloc(sizeof(plcConn));
	plcConnInit(conn);
	return sock;
}

static void process_msg_from_sock(int sock)
{
    struct sockaddr_un remote;
	/* msg is what we receive from gpdb */
	plcMessage *msg = NULL;
	/*
	 * container_msg is the message of our container creation result
	 * it will be sent back to gpdb.
	 */
	plcMsgContainer *container_msg;

    socklen_t len_addr = (socklen_t) sizeof(struct sockaddr_un);
    int s2 = accept(sock, &remote, &len_addr);
    if (s2 < 0) {
        return;
    }
	conn->sock = s2;
	int res = plcontainer_channel_receive(conn, &msg, MT_PLCID_BIT);
	if (res < 0) {
		elog(WARNING, "error in receiving request from QE");
		goto clean;
	}
	container_msg = (plcMsgContainer *) palloc(sizeof(plcMsgContainer));
	container_msg->msgtype = MT_PLC_CONTAINER;
	plcMsgPLCId *mplc_id = (plcMsgPLCId *)msg;
	elog(DEBUG1, "receive id msg from process %d", mplc_id->pid);
	container_msg->status = start_container(mplc_id->runtimeid, mplc_id->pid, mplc_id->sessionid, mplc_id->ccnt, &(container_msg->msg));
	plcontainer_channel_send(conn, (plcMessage *)container_msg);

clean:
    if (shutdown(s2, 2) < 0)
    {
        elog(ERROR, "error in shutdown uds");
    }
    close(s2);
}

static int wait_for_msg(int sock) {
    struct timeval timeout;
    int rv;
    fd_set fdset;

    FD_ZERO(&fdset);    /* clear the set */
    FD_SET(sock, &fdset); /* add our file descriptor to the set */
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    rv = select(sock + 1, &fdset, NULL, NULL, &timeout);

    if (rv == -1) {
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
		{
			elog(LOG, "select() socket got error: %s", strerror(errno));
			return 0;
		}
        elog(ERROR, "Failed to select() socket: %s", strerror(errno));
    }
    return 1;
}

void
plc_coordinator_main(Datum datum)
{
    int sock, rc;
	dsm_handle seg;

    (void)datum;
    pqsignal(SIGTERM, plc_coordinator_sigterm);
    pqsignal(SIGHUP, plc_coordinator_sighup);
	seg = shm_message_queue_sender_init();
    sock = plc_initialize_coordinator(seg);
    BackgroundWorkerUnblockSignals();

    coordinator_shm->state = CO_STATE_READY;
    elog(INFO, "plcoordinator is going to enter main loop, sock=%d", sock);
    while(!got_sigterm) {
        if (wait_for_msg(sock) > 0)
        {
            process_msg_from_sock(sock);
        }

        rc = WaitLatch(&MyProc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, 0);
        if (rc & WL_POSTMASTER_DEATH)
            break;
        ResetLatch(&MyProc->procLatch);
        if (got_sighup) {
            got_sighup = false;
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

    pqsignal(SIGTERM, plc_coordinator_sigterm);
    pqsignal(SIGHUP, SIG_IGN);
	/* TODO: error process */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "plcontainer monitor");
	seg = dsm_attach(DatumGetUInt32(datum));
	shm_message_queue_receiver_init(seg);
    BackgroundWorkerUnblockSignals();
    // TODO: impl coordinator logic here
    while(!got_sigterm) {
        rc = WaitLatch(&MyProc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, 0);
        if (rc & WL_POSTMASTER_DEATH)
            break;
        ResetLatch(&MyProc->procLatch);
		receive_message();
		sleep(2);
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

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
    worker.bgw_main = plc_coordinator_main;

    worker.bgw_notify_pid = 0;

    snprintf(worker.bgw_name, BGW_MAXLEN, "[plcontainer] - coordinator");

    RegisterBackgroundWorker(&worker);
    elog(NOTICE, "init plc_coordinator %d done", (int)getpid());
}

static int start_container(const char *runtimeid, pid_t qe_pid, int session_id, int ccnt, char **uds_address)
{
	/* debug test only */
	if (plcontainer_stand_alone_mode)
	{
		pid_t server_pid;
		QeRequest *request;
		int res;

		(void) runtimeid;
		*uds_address = (char*) palloc(1024);
		snprintf(*uds_address, 1024, "%s.%d.%d.%d.%d", DEBUG_UDS_PREFIX, qe_pid, session_id, ccnt, (int)getpid());
		server_pid = start_stand_alone_process(*uds_address);
		request = (QeRequest*) palloc (sizeof(QeRequest));
		request->pid = qe_pid;
		request->conn = session_id;
		request->requestType = CREATE_SERVER_DEBUG;
		snprintf(request->containerId, sizeof(request->containerId), "%d", server_pid);
		res = send_message(request);
		if (res != 0)
		{
			elog(WARNING, "send message failure");
			return -1;
		} else {
			elog(LOG, "send message success");
			return 0;
		}

	} else {
		return 0;
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

	/* Receive does not need to be blocked */
	res = shm_mq_receive(message_queue_handle, &nbytes, &data, true);

	if (res == SHM_MQ_WOULD_BLOCK)
	{
		elog(LOG, "PLC coordinator: no message received, wait for next trun");
		return 0;
	} else if (res == SHM_MQ_SUCCESS) {
		if (nbytes == sizeof(QeRequest))
		{
			QeRequest *request;

			request = (QeRequest*) data;

			elog(LOG, "PLC coordinator: receive message %d.%d --- %s", request->pid, request->conn, request->containerId);
		}
		return 0;
	} else {
		return -1;
	}
}

static int start_stand_alone_process(const char* uds_address)
{
	pid_t pid = 0;

	pid = fork();
	if (pid == 0)
		execl(plcontainer_stand_alone_server_path, plcontainer_stand_alone_server_path, uds_address, NULL);
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
