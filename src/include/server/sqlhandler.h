/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, 
 * 
 * This is a placehold function, and must NOT be called in server side
 *
 *------------------------------------------------------------------------------
 */

#ifndef FAKE_SQLHANDLER_H
#define FAKE_SQLHANDLER_H

#define deinit_pplan_slots(ignored) plc_elog(FATAL, \
                            "This function should not be called in server side!")

#define init_pplan_slots(ignored) plc_elog(FATAL, \
                            "This function should not be called in server side!")

#endif /* FAKE_SQLHANDLER_H */