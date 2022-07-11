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
#define MT_QUOTE          'Q'
#define MT_QUOTE_RESULT   'O'
#define MT_PING           'P'
#define MT_RESULT         'R'
#define MT_SQL            'S'
#define MT_TRIGREQ        'T'
#define MT_TUPLRES        'U'
#define MT_TRANSEVENT     'V'
#define MT_RAW            'W'
#define MT_SUBTRANSACTION 'N'
#define MT_SUBTRAN_RESULT 'Z'
// TODO QQQ server to client: send 'A': 1byte + AnyTable: 8byte + N: 4byte + N*TypeInfo
// TODO QQQ client to server: send 'A': 1byte + AnyTable: 8byte + method
//                         receive 'A': 1byte + N: 4byte + N*Tuple
#define MT_GP_ANYTABLE    'A'
#define MT_EOF            0

#define MT_CALLREQ_BIT        (1LL << 0)
#define MT_EXCEPTION_BIT      (1LL << 1)
#define MT_LOG_BIT            (1LL << 2)
#define MT_PING_BIT           (1LL << 3)
#define MT_RESULT_BIT         (1LL << 4)
#define MT_SQL_BIT            (1LL << 5)
#define MT_TRIGREQ_BIT        (1LL << 6)
#define MT_TUPLRES_BIT        (1LL << 7)
#define MT_TRANSEVENT_BIT     (1LL << 8)
#define MT_RAW_BIT            (1LL << 9)
#define MT_SUBTRANSACTION_BIT (1LL << 10)
#define MT_SUBTRAN_RESULT_BIT (1LL << 11)
#define MT_EOF_BIT            (1LL << 12)
#define MT_QUOTE_BIT          (1LL << 13)
#define MT_QUOTE_RESULT_BIT   (1LL << 14)
#define MT_GP_ANYTABLE_BIT    (1LL << 15)

#define MT_ALL_BITS        0xFFFFffffFFFFffffLL


#endif /* PLC_MESSAGE_TYPES_H */
