/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_BASE_H
#define PLC_MESSAGE_BASE_H

#include "../comm_utils.h"

#define base_message_content unsigned short msgtype;

typedef struct plcMessage {
    base_message_content
} plcMessage;

typedef struct {
    int32 isnull;
    char *value;
} rawdata;

/*
 * Note:
 * Must start from 0 since it is used as index to get type name.
 * See plc_get_type_name(). If you modify this, then you need to
 * modify plcDatatypeName[] correspondingly.
 */
typedef enum {
    PLC_DATA_INT1    = 0,  // 1-byte integer
    PLC_DATA_INT2,         // 2-byte integer
    PLC_DATA_INT4,         // 4-byte integer
    PLC_DATA_INT8,         // 8-byte integer
    PLC_DATA_FLOAT4,       // 4-byte float
    PLC_DATA_FLOAT8,       // 8-byte float
    PLC_DATA_TEXT,         // Text - transferred as a set of bytes of predefined length,
                           //        stored as cstring
    PLC_DATA_ARRAY,        // Array - array type specification should follow
    PLC_DATA_UDT,          // User-defined type, specification to follow
    PLC_DATA_BYTEA,        // Arbitrary set of bytes, stored and transferred as length + data
    PLC_DATA_INVALID,      // Invalid data type
    PLC_DATA_MAX
} plcDatatype;

typedef struct plcType plcType;

struct plcType {
    plcDatatype  type;
    int16        nSubTypes;
    char        *typeName;
    plcType     *subTypes;
};

typedef struct {
    plcType  type;
    char    *name;
    rawdata  data;
} plcArgument;

int plc_get_type_length(plcDatatype dt);
const char* plc_get_type_name(plcDatatype dt);

#endif /* PLC_MESSAGE_BASE_H */
