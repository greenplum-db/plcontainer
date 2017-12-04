/*
Copyright 1994 The PL-J Project. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE PL-J PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PL-J PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or implied, of the PL-J Project.
*/

/**
 * file name:            comm_channel.c
 * description:            Channel API.
 * author:            Laszlo Hornyak Kocka
 */

#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int message_start(plcConn *conn, char msgType);
static int message_end(plcConn *conn);
static int send_char(plcConn *conn, char c);
static int send_int16(plcConn *conn, int16 i);
static int send_int32(plcConn *conn, int32 i);
static int send_uint32(plcConn *conn, uint32 i);
static int send_int64(plcConn *conn, int64 i);
static int send_float4(plcConn *conn, float4 f);
static int send_float8(plcConn *conn, float8 f);
static int send_cstring(plcConn *conn, char *s);
static int send_bytea(plcConn *conn, char *s);
static int send_raw_object(plcConn *conn, plcType *type, rawdata *obj);
static int send_raw_array_iter(plcConn *conn, plcType *type, plcIterator *iter);
static int send_type(plcConn *conn, plcType *type);
static int send_udt(plcConn *conn, plcType *type, plcUDT *udt);
static int receive_message_type(plcConn *conn, char *c);
static int receive_char(plcConn *conn, char *c);
static int receive_int16(plcConn *conn, int16 *i);
static int receive_int32(plcConn *conn, int32 *i);
static int receive_uint32(plcConn *conn, uint32 *i);
static int receive_int64(plcConn *conn, int64 *i);
static int receive_float4(plcConn *conn, float4 *f);
static int receive_float8(plcConn *conn, float8 *f);
static int receive_raw(plcConn *conn, char *s, size_t len);
static int receive_cstring(plcConn *conn, char **s);
static int receive_bytea(plcConn *conn, char **s);
static int receive_raw_object(plcConn *conn, plcType *type, rawdata *obj);
static int receive_array(plcConn *conn, plcType *type, rawdata *obj);
static int receive_type(plcConn *conn, plcType *type);
static int receive_udt(plcConn *conn, plcType *type, char **resdata);
static int send_argument(plcConn *conn, plcArgument *arg);
static int send_ping(plcConn *conn);
static int send_call(plcConn *conn, plcMsgCallreq *call);
static int send_result(plcConn *conn, plcMsgResult *res);
static int send_log(plcConn *conn, plcMsgLog *mlog);
static int send_subtransaction(plcConn *conn, plcMsgSubtransaction *mSub);
static int send_subtransaction_result(plcConn *conn, plcMsgSubtransactionResult *mSubr);
static int send_exception(plcConn *conn, plcMsgError *err);
static int send_sql(plcConn *conn, plcMsgSQL *msg);
static int send_sql_statement(plcConn *conn, plcMsgSQL *msg);
static int send_sql_prepare(plcConn *conn, plcMsgSQL *msg);
static int send_sql_unprepare(plcConn *conn, plcMsgSQL *msg);
static int send_sql_pexecute(plcConn *conn, plcMsgSQL *msg);
static int send_rawmsg(plcConn *conn, plcMsgRaw *msg);
static int receive_exception(plcConn *conn, plcMessage **mExc);
static int receive_result(plcConn *conn, plcMessage **mRes);
static int receive_log(plcConn *conn, plcMessage **mLog);
static int receive_sql_statement(plcConn *conn, plcMessage **mStmt);
static int receive_sql_prepare(plcConn *conn, plcMessage **mStmt);
static int receive_sql_pexecute(plcConn *conn, plcMessage **mStmt);
static int receive_subtransaction(plcConn *conn, plcMessage **mSub);
static int receive_subtransaction_result(plcConn *conn, plcMessage **mSubr);
static int receive_argument(plcConn *conn, plcArgument *arg);
static int receive_ping(plcConn *conn, plcMessage **mPing);
static int receive_call(plcConn *conn, plcMessage **mCall);
static int receive_sql(plcConn *conn, plcMessage **mSql);
static int receive_rawmsg(plcConn *conn, plcMessage **mRaw);

/* Public API Functions */

int plcontainer_channel_send(plcConn *conn, plcMessage *msg) {
	int res;
	lprintf(DEBUG1, "start to send data, type is %c", msg->msgtype);
	switch (msg->msgtype) {
		case MT_PING:
			res = send_ping(conn);
			break;
		case MT_CALLREQ:
			res = send_call(conn, (plcMsgCallreq *) msg);
			break;
		case MT_RESULT:
			res = send_result(conn, (plcMsgResult *) msg);
			break;
		case MT_EXCEPTION:
			res = send_exception(conn, (plcMsgError *) msg);
			break;
		case MT_LOG:
			res = send_log(conn, (plcMsgLog *) msg);
			break;
		case MT_SQL:
			res = send_sql(conn, (plcMsgSQL *) msg);
			break;
		case MT_RAW:
			res = send_rawmsg(conn, (plcMsgRaw *) msg);
			break;
		case MT_SUBTRAN_RESULT:
			res = send_subtransaction_result(conn, (plcMsgSubtransactionResult *) msg);
			break;
		case MT_SUBTRANSACTION:
			res = send_subtransaction(conn, (plcMsgSubtransaction *) msg);
			break;
		default:
			lprintf(ERROR, "UNHANDLED MESSAGE: '%c'", msg->msgtype);
			res = -1;
			break;
	}
	return res;
}

/* Only receive for expected types. This helps memory recycling. */
int plcontainer_channel_receive(plcConn *conn, plcMessage **msg, int64 mask) {
	int res;
	char cType;

	conn->rx_timeout_sec = 0x7fffFFFF; /* Wait for ever at this moment */
	res = receive_message_type(conn, &cType);
	conn->rx_timeout_sec = TIMEOUT_SEC;
	lprintf(DEBUG1, "start to receive data, type is %c", cType);
	if (res >= 0) {
		switch (cType) {
			case MT_PING:
				if (!(mask & MT_PING_BIT))
					goto unexpected_type;
				res = receive_ping(conn, msg);
				break;
			case MT_CALLREQ:
				if (!(mask & MT_CALLREQ_BIT))
					goto unexpected_type;
				res = receive_call(conn, msg);
				break;
			case MT_RESULT:
				if (!(mask & MT_RESULT_BIT))
					goto unexpected_type;
				res = receive_result(conn, msg);
				break;
			case MT_EXCEPTION:
				if (!(mask & MT_EXCEPTION_BIT))
					goto unexpected_type;
				res = receive_exception(conn, msg);
				break;
			case MT_LOG:
				if (!(mask & MT_LOG_BIT))
					goto unexpected_type;
				res = receive_log(conn, msg);
				break;
			case MT_SQL:
				if (!(mask & MT_SQL_BIT))
					goto unexpected_type;
				res = receive_sql(conn, msg);
				break;
			case MT_RAW:
				if (!(mask & MT_RAW_BIT))
					goto unexpected_type;
				res = receive_rawmsg(conn, msg);
				break;
			case MT_SUBTRANSACTION:
				if (!(mask & MT_SUBTRANSACTION_BIT))
					goto unexpected_type;
				res = receive_subtransaction(conn, msg);
				break;
			case MT_SUBTRAN_RESULT:
				if (!(mask & MT_SUBTRAN_RESULT_BIT))
					goto unexpected_type;
				res = receive_subtransaction_result(conn, msg);
				break;
			default:
				lprintf(ERROR, "unknown message type: %d / '%c'", (int) cType, cType);
				*msg = NULL;
				return -1;
		}
	}
	return res;

	unexpected_type:
	/* If lprint with level < ERROR, we need to free the message for various types. */
	lprintf(ERROR, "unexpected message type: %d / '%c'. Mask of expected "
		    "message: 0x%llx", (int) cType, cType, (long long) mask);
	*msg = NULL;
	return -1;
}

/* Send-Receive for Primitive Datatypes */

static int message_start(plcConn *conn, char msgType) {
	return plcBufferAppend(conn, &msgType, 1);
}

static int message_end(plcConn *conn) {
	return plcBufferFlush(conn);
}

static int send_char(plcConn *conn, char c) {
	debug_print(WARNING, "    ===> sending int8/char '%d/%c'", (int) c, c);
	return plcBufferAppend(conn, &c, 1);
}

static int send_int16(plcConn *conn, int16 i) {
	debug_print(WARNING, "    ===> sending int16 '%d'", (int) i);
	return plcBufferAppend(conn, (char *) &i, 2);
}

static int send_int32(plcConn *conn, int32 i) {
	debug_print(WARNING, "    ===> sending int32 '%d'", i);
	return plcBufferAppend(conn, (char *) &i, 4);
}

static int send_uint32(plcConn *conn, uint32 i) {
	debug_print(WARNING, "    ===> sending uint32 '%u'", i);
	return plcBufferAppend(conn, (char *) &i, 4);
}

static int send_int64(plcConn *conn, int64 i) {
	debug_print(WARNING, "    ===> sending int64 '%lld'", i);
	return plcBufferAppend(conn, (char *) &i, 8);
}

static int send_float4(plcConn *conn, float4 f) {
	debug_print(WARNING, "    ===> sending float4 '%f'", f);
	return plcBufferAppend(conn, (char *) &f, 4);
}

static int send_float8(plcConn *conn, float8 f) {
	debug_print(WARNING, "    ===> sending float8 '%f'", f);
	return plcBufferAppend(conn, (char *) &f, 8);
}

static int send_cstring(plcConn *conn, char *s) {
	int res = 0;

	debug_print(WARNING, "    ===> sending cstring '%s'", s);
	if (s == NULL) {
		res = send_int32(conn, -1);
	} else {
		int cnt = strlen(s);

		res |= send_int32(conn, cnt);
		if (res == 0 && cnt > 0) {
			res = plcBufferAppend(conn, s, cnt);
		}
	}

	return res;
}

static int send_bytea(plcConn *conn, char *s) {
	int res = 0;

	debug_print(WARNING, "    ===> sending bytea of size '%d'", *((int *) s));
	res |= send_int32(conn, *((int *) s));
	res |= plcBufferAppend(conn, s + 4, *((int *) s));
	return res;
}

static int send_raw_object(plcConn *conn, plcType *type, rawdata *obj) {
	int res = 0;
	if (obj->isnull) {
		res |= send_char(conn, 'N');
		debug_print(WARNING, "Object is null");
	} else {
		res |= send_char(conn, 'D');
		debug_print(WARNING, "Object type is '%s' and value is:", plc_get_type_name(type->type));
		switch (type->type) {
			case PLC_DATA_INT1:
				res |= send_char(conn, *((char *) obj->value));
				break;
			case PLC_DATA_INT2:
				res |= send_int16(conn, *((int16 *) obj->value));
				break;
			case PLC_DATA_INT4:
				res |= send_int32(conn, *((int32 *) obj->value));
				break;
			case PLC_DATA_INT8:
				res |= send_int64(conn, *((int64 *) obj->value));
				break;
			case PLC_DATA_FLOAT4:
				res |= send_float4(conn, *((float4 *) obj->value));
				break;
			case PLC_DATA_FLOAT8:
				res |= send_float8(conn, *((float8 *) obj->value));
				break;
			case PLC_DATA_TEXT:
				res |= send_cstring(conn, obj->value);
				break;
			case PLC_DATA_BYTEA:
				res |= send_bytea(conn, obj->value);
				break;
			case PLC_DATA_ARRAY:
				res |= send_raw_array_iter(conn, &type->subTypes[0], (plcIterator *) obj->value);
				break;
			case PLC_DATA_UDT:
				res |= send_udt(conn, type, (plcUDT *) obj->value);
				break;
			default:
				lprintf(ERROR, "Received unsupported argument type: %s [%d]",
					    plc_get_type_name(type->type), type->type);
				break;
		}
	}
	return res;
}

static int send_raw_array_iter(plcConn *conn, plcType *type, plcIterator *iter) {
	int res = 0;
	int i = 0;
	plcArrayMeta *meta = (plcArrayMeta *) iter->meta;
	res |= send_int32(conn, meta->ndims);

	for (i = 0; i < meta->ndims; i++) {
		res |= send_int32(conn, meta->dims[i]);
	}
	for (i = 0; i < meta->size && res == 0; i++) {
		rawdata *raw_object = iter->next(iter);
		res |= send_raw_object(conn, type, raw_object);
		if (!raw_object->isnull) {
			if (type->type == PLC_DATA_UDT) {
				plc_free_udt((plcUDT *) raw_object->value, type, true);
			}
			pfree(raw_object->value);
		}
		pfree(raw_object);
	}
	if (iter->cleanup != NULL) {
		iter->cleanup(iter);
	}
	return res;
}

static int send_type(plcConn *conn, plcType *type) {
	int res = 0;
	int i = 0;

	debug_print(WARNING, "VVVVVVVVVVVVVVV");
	debug_print(WARNING, "    Type '%s' with name '%s'", plc_get_type_name(type->type), type->typeName);
	res |= send_char(conn, (char) type->type);
	res |= send_cstring(conn, type->typeName);
	if (type->type == PLC_DATA_ARRAY || type->type == PLC_DATA_UDT) {
		res |= send_int16(conn, type->nSubTypes);
		for (i = 0; i < type->nSubTypes && res == 0; i++)
			res |= send_type(conn, &type->subTypes[i]);
	}
	debug_print(WARNING, "///////////////");

	return res;
}

static int send_udt(plcConn *conn, plcType *type, plcUDT *udt) {
	int res = 0;
	int i = 0;

	debug_print(WARNING, "Sending user-defined type with %d members", type->nSubTypes);

	for (i = 0; i < type->nSubTypes && res == 0; i++) {
		res |= send_raw_object(conn, &type->subTypes[i], &udt->data[i]);
	}

	return res;
}

static int receive_message_type(plcConn *conn, char *c) {
	*c = '@';
	int res = plcBufferReceive(conn, 1);
	if (res == 0)
		res = plcBufferRead(conn, c, 1);
	return res;
}

static int receive_char(plcConn *conn, char *c) {
	int res = plcBufferRead(conn, c, 1);
	debug_print(WARNING, "    <=== receiving int8/char '%d/%c'", (int) *c, *c);
	return res;
}

static int receive_int16(plcConn *conn, int16 *i) {
	int res = plcBufferRead(conn, (char *) i, 2);
	debug_print(WARNING, "    <=== receiving int16 '%d'", (int) *i);
	return res;
}

static int receive_int32(plcConn *conn, int32 *i) {
	int res = plcBufferRead(conn, (char *) i, 4);
	debug_print(WARNING, "    <=== receiving int32 '%d'", *i);
	return res;
}

static int receive_uint32(plcConn *conn, uint32 *i) {
	int res = plcBufferRead(conn, (char *) i, 4);
	debug_print(WARNING, "    <=== receiving uint32 '%u'", *i);
	return res;
}

static int receive_int64(plcConn *conn, int64 *i) {
	int res = plcBufferRead(conn, (char *) i, 8);
	debug_print(WARNING, "    <=== receiving int64 '%lld'", *i);
	return res;
}

static int receive_float4(plcConn *conn, float4 *f) {
	int res = plcBufferRead(conn, (char *) f, 4);
	debug_print(WARNING, "    <=== receiving float4 '%f'", *f);
	return res;
}

static int receive_float8(plcConn *conn, float8 *f) {
	int res = plcBufferRead(conn, (char *) f, 8);
	debug_print(WARNING, "    <=== receiving float8 '%f'", *f);
	return res;
}

static int receive_raw(plcConn *conn, char *s, size_t len) {
	int res = plcBufferRead(conn, s, len);
	debug_print(WARNING, "    <=== receiving raw '%d' bytes", (int) len);
	return res;
}

static int receive_cstring(plcConn *conn, char **s) {
	int res = 0;
	int32 cnt;

	if (receive_int32(conn, &cnt) < 0) {
		return -1;
	}

	if (cnt == -1) {
		*s = NULL;
	} else if (cnt < 0) {
			lprintf(LOG, "receive_cstring() returns a negative length: %d", cnt);
		return -1;
	} else {
		*s = pmalloc(cnt + 1);
		if (cnt > 0) {
			res = plcBufferRead(conn, *s, cnt);
		}
		(*s)[cnt] = 0;
	}

	debug_print(WARNING, "    <=== receiving cstring '%s'", *s);
	return res;
}

static int receive_bytea(plcConn *conn, char **s) {
	int res = 0;
	int len = 0;

	if (receive_int32(conn, &len) < 0) {
		return -1;
	}

	*s = pmalloc(len + 4);
	debug_print(WARNING, "    ===> receiving bytea of size '%d' at %p for %p", len, *s, s);

	*((int *) *s) = len;
	if (len > 0) {
		res = plcBufferRead(conn, *s + 4, len);
	}
	debug_print(WARNING, "    ===> receiving bytea '%s'", strndup(*s + 4, len));

	return res;
}

static int receive_raw_object(plcConn *conn, plcType *type, rawdata *obj) {
	int res = 0;
	char isn;
	if (obj == NULL) {
		lprintf(ERROR, "NULL object reference received by receive_raw_object");
	}
	res |= receive_char(conn, &isn);
	if (isn == 'N') {
		obj->isnull = 1;
		obj->value = NULL;
		debug_print(WARNING, "Object is null");
	} else {
		obj->isnull = 0;
		debug_print(WARNING, "Object value is:");
		switch (type->type) {
			case PLC_DATA_INT1:
				obj->value = (char *) pmalloc(1);
				res |= receive_char(conn, (char *) obj->value);
				break;
			case PLC_DATA_INT2:
				obj->value = (char *) pmalloc(2);
				res |= receive_int16(conn, (int16 *) obj->value);
				break;
			case PLC_DATA_INT4:
				obj->value = (char *) pmalloc(4);
				res |= receive_int32(conn, (int32 *) obj->value);
				break;
			case PLC_DATA_INT8:
				obj->value = (char *) pmalloc(8);
				res |= receive_int64(conn, (int64 *) obj->value);
				break;
			case PLC_DATA_FLOAT4:
				obj->value = (char *) pmalloc(4);
				res |= receive_float4(conn, (float4 *) obj->value);
				break;
			case PLC_DATA_FLOAT8:
				obj->value = (char *) pmalloc(8);
				res |= receive_float8(conn, (float8 *) obj->value);
				break;
			case PLC_DATA_TEXT:
				res |= receive_cstring(conn, &obj->value);
				break;
			case PLC_DATA_BYTEA:
				res |= receive_bytea(conn, &obj->value);
				break;
			case PLC_DATA_ARRAY:
				res |= receive_array(conn, &type->subTypes[0], obj);
				break;
			case PLC_DATA_UDT:
				res |= receive_udt(conn, type, &obj->value);
				break;
			default:
				lprintf(ERROR, "Received unsupported argument type: %s [%d]",
					    plc_get_type_name(type->type), type->type);
				break;
		}
	}
	return res;
}

static int receive_array(plcConn *conn, plcType *type, rawdata *obj) {
	int res = 0;
	int i = 0;
	int ndims;
	int entrylen = 0;
	char isnull;
	plcArray *arr;

	res |= receive_int32(conn, &ndims);
	arr = plc_alloc_array(ndims);
	obj->value = (char *) arr;
	arr->meta->type = (plcDatatype) ((int) type->type);
	arr->meta->size = ndims > 0 ? 1 : 0;
	for (i = 0; i < ndims; i++) {
		res |= receive_int32(conn, &arr->meta->dims[i]);
		arr->meta->size *= arr->meta->dims[i];
	}
	if (arr->meta->size > 0) {
		entrylen = plc_get_type_length(arr->meta->type);
		arr->nulls = (char *) pmalloc(arr->meta->size * 1);
		arr->data = (char *) pmalloc(arr->meta->size * entrylen);
		memset(arr->data, 0, arr->meta->size * entrylen);

		for (i = 0; i < arr->meta->size && res == 0; i++) {
			res |= receive_char(conn, &isnull);
			if (isnull == 'N') {
				arr->nulls[i] = 1;
			} else {
				arr->nulls[i] = 0;
				switch (arr->meta->type) {
					case PLC_DATA_INT1:
					case PLC_DATA_INT2:
					case PLC_DATA_INT4:
					case PLC_DATA_INT8:
					case PLC_DATA_FLOAT4:
					case PLC_DATA_FLOAT8:
						res |= receive_raw(conn, arr->data + i * entrylen, entrylen);
						break;
					case PLC_DATA_TEXT:
						res |= receive_cstring(conn, &((char **) arr->data)[i]);
						break;
					case PLC_DATA_BYTEA:
						res |= receive_bytea(conn, &((char **) arr->data)[i]);
						break;
					case PLC_DATA_UDT:
						res |= receive_udt(conn, type, &((char **) arr->data)[i]);
						break;
					default:
						lprintf(ERROR, "Should not get here (type: %d)",
							    arr->meta->type);
						break;
				}
			}
		}
	}
	return res;
}

static int receive_type(plcConn *conn, plcType *type) {
	int res = 0;
	int i = 0;
	char typ;

	debug_print(WARNING, "VVVVVVVVVVVVVVV");
	res |= receive_char(conn, &typ);
	res |= receive_cstring(conn, &type->typeName);
	type->type = (int) typ;
	debug_print(WARNING, "    Type '%s' with name '%s'", plc_get_type_name(type->type), type->typeName);

	if (type->type == PLC_DATA_ARRAY || type->type == PLC_DATA_UDT) {
		res |= receive_int16(conn, &type->nSubTypes);
		if (type->nSubTypes > 0) {
			type->subTypes = (plcType *) pmalloc(type->nSubTypes * sizeof(plcType));
			for (i = 0; i < type->nSubTypes && res == 0; i++)
				res |= receive_type(conn, &type->subTypes[i]);
		}
	} else {
		type->nSubTypes = 0;
		type->subTypes = NULL;
	}
	debug_print(WARNING, "///////////////");

	return res;
}

static int receive_udt(plcConn *conn, plcType *type, char **resdata) {
	int res = 0;
	int i = 0;
	plcUDT *udt;

	debug_print(WARNING, "Receiving user-defined type with %d members", type->nSubTypes);

	udt = plc_alloc_udt(type->nSubTypes);
	for (i = 0; i < type->nSubTypes && res == 0; i++) {
		res |= receive_raw_object(conn, &type->subTypes[i], &udt->data[i]);
	}

	*resdata = (char *) udt;
	return res;
}

/* Send Functions for the Main Engine */

static int send_argument(plcConn *conn, plcArgument *arg) {
	int res = 0;
	debug_print(WARNING, "Sending argument '%s'", arg->name);
	res |= send_cstring(conn, arg->name);
	debug_print(WARNING, "Argument type is '%s'", plc_get_type_name(arg->type.type));
	res |= send_type(conn, &arg->type);
	res |= send_raw_object(conn, &arg->type, &arg->data);
	return res;
}

static int send_ping(plcConn *conn) {
	int res = 0;
	char *ping = "ping";

	debug_print(WARNING, "Sending ping message");
	res |= message_start(conn, MT_PING);
	res |= send_cstring(conn, ping);
	res |= message_end(conn);
	debug_print(WARNING, "Finished ping message");
	return res;
}

static int send_call(plcConn *conn, plcMsgCallreq *call) {
	int res = 0;
	int i;

	debug_print(WARNING, "Sending call request for function '%s'", call->proc.name);
	res |= message_start(conn, MT_CALLREQ);
	res |= send_cstring(conn, call->proc.name);
	debug_print(WARNING, "Function source code:");
	debug_print(WARNING, "%s", call->proc.src);
	res |= send_cstring(conn, call->proc.src);
	debug_print(WARNING, "Function OID is '%u'", call->objectid);
	res |= send_uint32(conn, call->objectid);
	debug_print(WARNING, "Function has changed is '%d'", call->hasChanged);
	res |= send_int32(conn, call->hasChanged);
	debug_print(WARNING, "Function return type is '%s'", plc_get_type_name(call->retType.type));
	res |= send_type(conn, &call->retType);
	debug_print(WARNING, "Function is set-returning: %d", (int) call->retset);
	res |= send_int32(conn, call->retset);
	debug_print(WARNING, "Function number of arguments is '%d'", call->nargs);
	res |= send_int32(conn, call->nargs);

	for (i = 0; i < call->nargs; i++)
		res |= send_argument(conn, &call->args[i]);

	res |= message_end(conn);
	debug_print(WARNING, "Finished call request for function '%s'", call->proc.name);
	return res;
}

static int send_result(plcConn *conn, plcMsgResult *ret) {
	int res = 0;
	uint32 i, j;
	plcMsgError *msg = NULL;

	res |= message_start(conn, MT_RESULT);
	debug_print(WARNING, "Sending result of %d rows and %d columns", ret->rows, ret->cols);
	res |= send_int32(conn, ret->rows);
	res |= send_int32(conn, ret->cols);

	/* send columns types and names */
	debug_print(WARNING, "Sending types and names of %d columns", ret->cols);
	for (i = 0; i < ret->cols; i++) {
		debug_print(WARNING, "Column '%s' with type '%d'", ret->names[i], (int) ret->types[i].type);
		res |= send_type(conn, &ret->types[i]);
		res |= send_cstring(conn, ret->names[i]);
	}

	/* send rows */
	for (i = 0; i < ret->rows; i++)
		for (j = 0; j < ret->cols; j++) {
			debug_print(WARNING, "Sending row %d column %d", i, j);
			res |= send_raw_object(conn, &ret->types[j], &ret->data[i][j]);
		}

	if (ret->exception_callback != NULL) {
		msg = (plcMsgError *) ret->exception_callback();
	}

	if (msg == NULL) {
		res |= send_char(conn, 'N');
	} else {
		res |= send_exception(conn, msg);
		free_error(msg);
	}

	res |= message_end(conn);

	debug_print(WARNING, "Finished sending function result");

	return res;
}

static int send_log(plcConn *conn, plcMsgLog *mlog) {
	int res = 0;

	debug_print(WARNING, "Sending log message to backend");
	res |= message_start(conn, MT_LOG);
	res |= send_int32(conn, mlog->level);
	res |= send_cstring(conn, mlog->message);

	res |= message_end(conn);
	debug_print(WARNING, "Finished sending log message");
	return res;
}


static int send_subtransaction_result(plcConn *conn, plcMsgSubtransactionResult *mSubr) {
	int res = 0;

	debug_print(WARNING, "Sending subtransaction result message to client");
	res |= message_start(conn, MT_SUBTRAN_RESULT);
	res |= send_int16(conn, mSubr->result);

	res |= message_end(conn);
	debug_print(WARNING, "Finished sending subtransaction result message");
	return res;
}

static int send_subtransaction(plcConn *conn, plcMsgSubtransaction *mSub) {
	int res = 0;

	debug_print(WARNING, "Sending subtransaction result message to backend");
	res |= message_start(conn, MT_SUBTRANSACTION);
	res |= send_char(conn, mSub->action);
	res |= send_char(conn, mSub->type);

	res |= message_end(conn);
	debug_print(WARNING, "Finished sending subtransaction message");
	return res;
}


static int send_exception(plcConn *conn, plcMsgError *err) {
	int res = 0;
	res |= message_start(conn, MT_EXCEPTION);
	res |= send_cstring(conn, err->message);
	res |= send_cstring(conn, err->stacktrace);
	res |= message_end(conn);
	return res;
}

static int send_sql(plcConn *conn, plcMsgSQL *msg) {
	int res;

	switch (msg->sqltype) {
		case SQL_TYPE_STATEMENT:
			res = send_sql_statement(conn, msg);
			break;
		case SQL_TYPE_PREPARE:
			res = send_sql_prepare(conn, msg);
			break;
		case SQL_TYPE_UNPREPARE:
			res = send_sql_unprepare(conn, msg);
			break;
		case SQL_TYPE_PEXECUTE:
			res = send_sql_pexecute(conn, msg);
			break;
		default:
			res = -1;
			lprintf(ERROR, "UNHANDLED SQL TYPE: %d for sql send", msg->sqltype);
			break;
	}

	return res;
}

static int send_sql_statement(plcConn *conn, plcMsgSQL *msg) {
	int res = 0;

	res |= message_start(conn, MT_SQL);
	res |= send_int32(conn, msg->sqltype);

	res |= send_int64(conn, msg->limit);
	res |= send_cstring(conn, msg->statement);
	res |= message_end(conn);

	return res;
}

static int send_sql_prepare(plcConn *conn, plcMsgSQL *msg) {
	int res = 0;
	int i;

	res |= message_start(conn, MT_SQL);
	res |= send_int32(conn, msg->sqltype);

	res |= send_int32(conn, msg->nargs);
	for (i = 0; i < msg->nargs; i++)
		res |= send_argument(conn, &msg->args[i]);

	res |= send_cstring(conn, msg->statement);
	res |= message_end(conn);

	return res;
}

static int send_sql_unprepare(plcConn *conn, plcMsgSQL *msg) {
	int res = 0;

	res |= message_start(conn, MT_SQL);
	res |= send_int32(conn, msg->sqltype);
	res |= send_int64(conn, (int64) msg->pplan);
	res |= message_end(conn);

	return res;
}

static int send_sql_pexecute(plcConn *conn, plcMsgSQL *msg) {
	int res = 0;
	int i;

	res |= message_start(conn, MT_SQL);
	res |= send_int32(conn, msg->sqltype);

	res |= send_int32(conn, msg->nargs);
	for (i = 0; i < msg->nargs; i++)
		res |= send_argument(conn, &msg->args[i]);
	res |= send_int64(conn, msg->limit);

	res |= send_int64(conn, (int64) msg->pplan);
	res |= message_end(conn);

	return res;
}

static int send_rawmsg(plcConn *conn, plcMsgRaw *msg) {
	int res = 0;
	int i;

	res |= message_start(conn, MT_RAW);
	res |= send_int32(conn, msg->size);
	for (i = 0; i < msg->size; i++)
		res |= send_char(conn, msg->data[i]);
	res |= message_end(conn);

	return res;
}

/* Receive Functions for the Main Engine */

static int receive_exception(plcConn *conn, plcMessage **mExc) {
	int res = 0;
	plcMsgError *ret;

	*mExc = pmalloc(sizeof(plcMsgError));
	ret = (plcMsgError *) *mExc;
	ret->msgtype = MT_EXCEPTION;
	res |= receive_cstring(conn, &ret->message);
	res |= receive_cstring(conn, &ret->stacktrace);

	return res;
}

static int receive_result(plcConn *conn, plcMessage **mRes) {
	uint32 i, j;
	int res = 0;
	char exc;
	plcMsgResult *ret;

	*mRes = pmalloc(sizeof(plcMsgResult));
	ret = (plcMsgResult *) *mRes;
	ret->msgtype = MT_RESULT;
	res |= receive_uint32(conn, &ret->rows);
	res |= receive_uint32(conn, &ret->cols);
	debug_print(WARNING, "Receiving function result of %d rows and %d columns",
	            ret->rows, ret->cols);

	if (res == 0) {
		ret->data = NULL;
		ret->types = NULL;
		ret->names = NULL;

		/* Receive data, if ret->cols == 0, it is update/delete/insert, no data need to receive */
		debug_print(WARNING, "Receiving types and names of %d rows * %d columns", ret->rows, ret->cols);
		/* receive columns types and names */
		if (ret->cols > 0) {
			ret->types = pmalloc(ret->cols * sizeof(plcType));
			ret->names = pmalloc(ret->cols * sizeof(*ret->names));

			for (i = 0; i < ret->cols && res == 0; i++) {
				res |= receive_type(conn, &ret->types[i]);
				res |= receive_cstring(conn, &ret->names[i]);
				debug_print(WARNING, "Column '%s' with type '%d'", ret->names[i], (int) ret->types[i].type);
			}
			for (; i < ret->cols; i++) {
				ret->types[i].typeName = NULL;
				ret->types[i].nSubTypes = 0;
				ret->names[i] = NULL;
			}

			/* receive rows */
			if (ret->rows > 0) {
				ret->data = pmalloc(ret->rows * sizeof(rawdata *));

				for (i = 0; i < ret->rows && res == 0; i++) {
					ret->data[i] = pmalloc(ret->cols * sizeof(*ret->data[i]));
					for (j = 0; j < ret->cols; j++) {
						debug_print(WARNING, "Receiving row %d column %d", i, j);
						res |= receive_raw_object(conn, &ret->types[j], &ret->data[i][j]);
					}
				}
				for (; i < ret->rows; i++)
					ret->data[i] = NULL;
			}
		}
	}

	res |= receive_char(conn, &exc);
	if (exc == MT_EXCEPTION) {
		res |= receive_exception(conn, mRes);
		free_result(ret, false);
	}

	debug_print(WARNING, "Finished receiving function result");
	return res;
}

static int receive_log(plcConn *conn, plcMessage **mLog) {
	int res = 0;
	plcMsgLog *ret;

	debug_print(WARNING, "Receiving log message from client");
	*mLog = pmalloc(sizeof(plcMsgLog));
	ret = (plcMsgLog *) *mLog;
	ret->msgtype = MT_LOG;
	res |= receive_int32(conn, &ret->level);
	res |= receive_cstring(conn, &ret->message);

	debug_print(WARNING, "Finished receiving log message");
	return res;
}

static int receive_sql_statement(plcConn *conn, plcMessage **mStmt) {
	int res = 0;
	plcMsgSQL *ret;

	*mStmt = pmalloc(sizeof(plcMsgSQL));
	ret = (plcMsgSQL *) *mStmt;
	ret->msgtype = MT_SQL;
	ret->sqltype = SQL_TYPE_STATEMENT;
	res = receive_int64(conn, &ret->limit);
	res |= receive_cstring(conn, &ret->statement);
	return res;
}

static int receive_sql_prepare(plcConn *conn, plcMessage **mStmt) {
	int i, res = 0;
	plcMsgSQL *ret;

	*mStmt = pmalloc(sizeof(plcMsgSQL));
	ret = (plcMsgSQL *) *mStmt;
	ret->msgtype = MT_SQL;
	ret->sqltype = SQL_TYPE_PREPARE;

	debug_print(WARNING, "Receiving spi prepare request");
	res |= receive_int32(conn, &ret->nargs);
	if (ret->nargs < 0) {
		lprintf(LOG, "spi prepare request with nargs (%d) < 0", ret->nargs);
		return -1;
	} else if (ret->nargs > 0) {
		ret->args = pmalloc(ret->nargs * sizeof(*ret->args));
		for (i = 0; i < ret->nargs; i++)
			res |= receive_argument(conn, &ret->args[i]);
	}

	res |= receive_cstring(conn, &ret->statement);

	debug_print(WARNING, "Received spi prepare request and returned %d", res);
	return res;
}

static int receive_sql_unprepare(plcConn *conn, plcMessage **mStmt) {
	int res = 0;
	int64 pplan;
	plcMsgSQL *ret;

	*mStmt = pmalloc(sizeof(plcMsgSQL));
	ret = (plcMsgSQL *) *mStmt;
	ret->msgtype = MT_SQL;
	ret->sqltype = SQL_TYPE_UNPREPARE;

	debug_print(WARNING, "Receiving spi unprepare request");
	res |= receive_int64(conn, &pplan);
	ret->pplan = (void *) pplan;

	debug_print(WARNING, "Received spi unprepare request and returned %d", res);
	return res;
}

static int receive_subtransaction(plcConn *conn, plcMessage **mSub) {
	int res = 0;
	plcMsgSubtransaction *ret;
	*mSub = pmalloc(sizeof(plcMsgSubtransaction));
	ret = (plcMsgSubtransaction *) *mSub;
	ret->msgtype = MT_SUBTRANSACTION;

	debug_print(WARNING, "Receiving subtransaction request");
	res |= receive_char(conn, &ret->action);
	res |= receive_char(conn, &ret->type);
	debug_print(WARNING, "Received subtransaction request and returned %d", res);

	return res;
}

static int receive_subtransaction_result(plcConn *conn, plcMessage **mSubr) {
	int res = 0;
	plcMsgSubtransactionResult *ret;
	*mSubr = pmalloc(sizeof(plcMsgSubtransactionResult));
	ret = (plcMsgSubtransactionResult *) *mSubr;
	ret->msgtype = MT_SUBTRAN_RESULT;

	debug_print(WARNING, "Receiving subtransaction result");
	res |= receive_int16(conn, &ret->result);
	debug_print(WARNING, "Received subtransaction result and returned %d", res);

	return res;
}

static int receive_sql_pexecute(plcConn *conn, plcMessage **mStmt) {
	int res = 0;
	int64 pplan;
	plcMsgSQL *ret;
	int i;

	*mStmt = pmalloc(sizeof(plcMsgSQL));
	ret = (plcMsgSQL *) *mStmt;
	ret->msgtype = MT_SQL;
	ret->sqltype = SQL_TYPE_PEXECUTE;

	debug_print(WARNING, "Receiving spi pexecute request");
	res |= receive_int32(conn, &ret->nargs);
	if (ret->nargs < 0) {
		lprintf(LOG, "spi pexecute request with nargs (%d) < 0", ret->nargs);
		return -1;
	} else if (ret->nargs > 0) {
		ret->args = pmalloc(ret->nargs * sizeof(*ret->args));
		for (i = 0; i < ret->nargs; i++)
			res |= receive_argument(conn, &ret->args[i]);
	}
	res |= receive_int64(conn, &ret->limit);
	res |= receive_int64(conn, &pplan);
	ret->pplan = (void *) pplan;

	debug_print(WARNING, "Received spi pexecute request and returned %d", res);
	return res;
}

static int receive_argument(plcConn *conn, plcArgument *arg) {
	int res = 0;
	res |= receive_cstring(conn, &arg->name);
	debug_print(WARNING, "Receiving argument '%s'", arg->name);
	res |= receive_type(conn, &arg->type);
	debug_print(WARNING, "Argument type is '%s'", plc_get_type_name(arg->type.type));
	res |= receive_raw_object(conn, &arg->type, &arg->data);
	return res;
}

static int receive_ping(plcConn *conn, plcMessage **mPing) {
	int res = 0;
	char *ping;

	*mPing = (plcMessage *) pmalloc(sizeof(plcMsgPing));
	((plcMsgPing *) *mPing)->msgtype = MT_PING;

	debug_print(WARNING, "Receiving ping message");
	res |= receive_cstring(conn, &ping);
	if (res == 0) {
		if (strncmp(ping, "ping", strlen("ping")) != 0) {
			debug_print(WARNING, "Ping message receive failed");
			res = -1;
		}
		pfree(ping);
	}

	debug_print(WARNING, "Finished receiving ping message");
	return res;
}

static int receive_call(plcConn *conn, plcMessage **mCall) {
	int res = 0;
	int i;
	plcMsgCallreq *req;

	*mCall = pmalloc(sizeof(plcMsgCallreq));
	req = (plcMsgCallreq *) *mCall;
	req->msgtype = MT_CALLREQ;
	res |= receive_cstring(conn, &req->proc.name);
	debug_print(WARNING, "Receiving call request for function '%s'", req->proc.name);
	res |= receive_cstring(conn, &req->proc.src);
	debug_print(WARNING, "Function source code:");
	debug_print(WARNING, "%s", req->proc.src);
	res |= receive_uint32(conn, &req->objectid);
	debug_print(WARNING, "Function OID is '%u'", req->objectid);
	res |= receive_int32(conn, &req->hasChanged);
	debug_print(WARNING, "Function has changed is '%d'", req->hasChanged);
	res |= receive_type(conn, &req->retType);
	debug_print(WARNING, "Function return type is '%s'", plc_get_type_name(req->retType.type));
	res |= receive_int32(conn, &req->retset);
	debug_print(WARNING, "Function is set-returning: %d", (int) req->retset);
	res |= receive_int32(conn, &req->nargs);
	debug_print(WARNING, "Function number of arguments is '%d'", req->nargs);
	if (res == 0) {
		req->args = NULL;
		if (req->nargs > 0) {
			req->args = pmalloc(sizeof(*req->args) * req->nargs);
			for (i = 0; i < req->nargs && res == 0; i++)
				res |= receive_argument(conn, &req->args[i]);
		} else if (req->nargs < 0) {
			lprintf(LOG, "function call with nargs (%d) < 0", req->nargs);
			return -1;
		}
	}
	debug_print(WARNING, "Finished call request for function '%s'", req->proc.name);
	return res;
}

static int receive_sql(plcConn *conn, plcMessage **mSql) {
	int res = 0;
	int sqlType;

	res = receive_int32(conn, &sqlType);
	if (res == 0) {
		switch (sqlType) {
			case SQL_TYPE_STATEMENT:
				res = receive_sql_statement(conn, mSql);
				break;
			case SQL_TYPE_PREPARE:
				res = receive_sql_prepare(conn, mSql);
				break;
			case SQL_TYPE_UNPREPARE:
				res = receive_sql_unprepare(conn, mSql);
				break;
			case SQL_TYPE_PEXECUTE:
				res = receive_sql_pexecute(conn, mSql);
				break;
			default:
				res = -1;
				lprintf(ERROR, "UNHANDLED SQL TYPE: %d for sql receive", sqlType);
				break;
		}
	}
	return res;
}

static int receive_rawmsg(plcConn *conn, plcMessage **mRaw) {
	int res = 0;
	plcMsgRaw *ret;

	*mRaw = (plcMessage *) pmalloc(sizeof(plcMsgRaw));
	ret = (plcMsgRaw *) *mRaw;
	ret->msgtype = MT_RAW;

	res |= receive_int32(conn, &ret->size);
	if (ret->size < 0) {
		return -1;
	} else {
		ret->data = (char *) pmalloc(ret->size);
		res |= receive_raw(conn, ret->data, ret->size);
	}

	return res;
}

void fill_prepare_argument(plcArgument *arg, char *str, plcDatatype plcData) {
	if (arg == NULL || str == NULL)
		lprintf (ERROR, "Impossible to reach here for spi prepare: %p, %p",
			    arg, str);

	if (str != NULL)
		arg->type.typeName = pstrdup(str);
	if (arg->type.typeName == NULL)
		lprintf (ERROR, "Failed to allocate memory for spi prepare (size: %zd)",
			    strlen(str));

	arg->type.type = plcData;

	/* Make free_arguments() happy. */
	arg->type.nSubTypes = 0;
	arg->name = NULL;
	arg->data.isnull = 1;
	arg->data.value = NULL;
}
