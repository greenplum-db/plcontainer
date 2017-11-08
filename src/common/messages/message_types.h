/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_TYPES_H
#define PLC_MESSAGE_TYPES_H

#define MT_CALLREQ        'C'
#define MT_EXCEPTION      'E'
#define MT_LOG            'L'
#define MT_PING           'P'
#define MT_RESULT         'R'
#define MT_SQL            'S'
#define MT_TRIGREQ        'T'
#define MT_TUPLRES        'U'
#define MT_TRANSEVENT     'V'
#define MT_RAW            'W'
#define MT_SUBTRANSACTION 'N'
#define MT_SUBTRAN_RESULT 'Z'
#define MT_EOF            0

#define MT_CALLREQ_BIT        0x1LL
#define MT_EXCEPTION_BIT      0x2LL
#define MT_LOG_BIT            0x4LL
#define MT_PING_BIT           0x8LL
#define MT_RESULT_BIT         0x10LL
#define MT_SQL_BIT            0x20LL
#define MT_TRIGREQ_BIT        0x40LL
#define MT_TUPLRES_BIT        0x80LL
#define MT_TRANSEVENT_BIT     0x100LL
#define MT_RAW_BIT            0x200LL
#define MT_SUBTRANSACTION_BIT 0x400LL
#define MT_SUBTRAN_RESULT_BIT 0x800LL
#define MT_EOF_BIT            0x1000LL

#define MT_ALL_BITS        0xFFFFffffFFFFffffLL


#endif /* PLC_MESSAGE_TYPES_H */
