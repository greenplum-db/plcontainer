/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef _CO_COORDINATOR_H
#define _CO_COORDINATOR_H





typedef struct requester_info_entry
{
    int id; // pid of QE
    int sock; // socket file descriptor between QE and coordinator
} requester_info_entry;

#endif /* _CO_COORDINATOR_H */
