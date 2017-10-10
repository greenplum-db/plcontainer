/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_TYPEIO_H
#define PLC_TYPEIO_H

#include "postgres.h"
#include "funcapi.h"

#include "common/messages/messages.h"
#include "plcontainer.h"

typedef struct plcTypeInfo plcTypeInfo;
typedef char *(*plcDatumOutput)(Datum, plcTypeInfo*);
typedef Datum (*plcDatumInput)(char*, plcTypeInfo*);

struct plcTypeInfo {
    /* PL/Container-specific information */
    plcDatatype     type;
    int             nSubTypes;
    plcTypeInfo    *subTypes;

    /* Custom input and output functions used for most common data types that
     * allow binary data transfer */
    plcDatumOutput  outfunc;
    plcDatumInput   infunc;

    /* GPDB in- and out- functions to transform custom types to text and back */
    RegProcedure    output, input;

    /* Information used for type input/output operations */
    Oid             typeOid;
    Oid             typelem;
    bool            typbyval;
    int16           typlen;
    char            typalign;
    int32           typmod;

    /* UDT-specific information */
    bool            is_rowtype;
    bool            is_record;
    bool            attisdropped;
    Oid             typ_relid;
    TransactionId   typrel_xmin;
    ItemPointerData typrel_tid;
    char           *typeName;
};

typedef struct plcPgArrayPosition {
    plcTypeInfo    *type;
    bits8          *bitmap;
    int             bitmask;
} plcPgArrayPosition;

void fill_type_info(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type);
void copy_type_info(plcType *type, plcTypeInfo *ptype);
void free_type_info(plcTypeInfo *type);
char *fill_type_value(Datum funcArg, plcTypeInfo *argType);
plcDatatype plc_get_datatype_from_oid(Oid oid);

#endif /* PLC_TYPEIO_H */
