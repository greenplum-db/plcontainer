/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef _CO_COORDINATOR_H
#define _CO_COORDINATOR_H

/* the container status key */
typedef struct ContainerKey
{
	pid_t       qe_pid;
	int         conn;
	int 		ccnt;
} ContainerKey;

/* the container status entry */
typedef struct ContainerEntry
{
	ContainerKey    key;		        /* hash key */
	char            containerId[16];    /* for container */
	pid_t           stand_alone_pid;    /* for stand alone mode */
	char* 			status;
} ContainerEntry;

#endif /* _CO_COORDINATOR_H */
