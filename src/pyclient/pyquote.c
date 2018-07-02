/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "pycall.h"
#include "pyquote.h"
#include "common/messages/messages.h"
#include "common/comm_channel.h"
#include "common/comm_utils.h"

#include <Python.h>

PyObject *
PLy_quote_literal(PyObject *self UNUSED, PyObject *args)
{
	plcConn *conn = plcconn_global;
	plcMsgQuote *msg;
	plcMessage *resp = NULL;
	const char *str;
	char	   *quoted;
	PyObject   *ret;

	if (!PyArg_ParseTuple(args, "s", &str))
		return NULL;

	msg = pmalloc(sizeof(plcMsgQuote));
	msg->msgtype = MT_QUOTE;
	msg->quote_type = QUOTE_TYPE_LITERAL;
	msg->msg = strdup(str);
	plcontainer_channel_send(conn, (plcMessage *) msg);
	plcontainer_channel_receive(conn, &resp, MT_QUOTE_RESULT_BIT);
	quoted = strdup(((plcMsgQuoteResult *)resp)->result);
	ret = PyString_FromString(quoted);
	pfree(quoted);

	return ret;
}

PyObject *
PLy_quote_nullable(PyObject *self UNUSED, PyObject *args)
{
	plcConn *conn = plcconn_global;
	plcMsgQuote *msg;
	plcMessage *resp = NULL;
	const char *str;
	char	   *quoted;
	PyObject   *ret;

	if (!PyArg_ParseTuple(args, "z", &str))
		return NULL;

	if (str == NULL)
		return PyString_FromString("NULL");

	msg = pmalloc(sizeof(plcMsgQuote));
	msg->msgtype = MT_QUOTE;
	msg->quote_type = QUOTE_TYPE_NULLABLE;
	msg->msg = strdup(str);
	plcontainer_channel_send(conn, (plcMessage *) msg);
	plcontainer_channel_receive(conn, &resp, MT_QUOTE_RESULT_BIT);
	quoted = strdup(((plcMsgQuoteResult *)resp)->result);
	ret = PyString_FromString(quoted);
	pfree(quoted);

	return ret;
}

PyObject *
PLy_quote_ident(PyObject *self UNUSED, PyObject *args)
{
	plc_elog(LOG, "Client has step into quote_ident");
	plcConn *conn = plcconn_global;
	plcMsgQuote *msg;
	plcMessage *resp = NULL;
	const char *str;
	char *quoted;
	PyObject   *ret;

	if (!PyArg_ParseTuple(args, "s", &str))
		return NULL;

	msg = pmalloc(sizeof(plcMsgQuote));
	msg->msgtype = MT_QUOTE;
	msg->quote_type = QUOTE_TYPE_IDENT;
	msg->msg = strdup(str);
	plc_elog(LOG, "Client has step before plcontainer_channel_send");
	plcontainer_channel_send(conn, (plcMessage *) msg);
	plc_elog(LOG, "Client has step before plcontainer_channel_receive");
	plcontainer_channel_receive(conn, &resp, MT_QUOTE_RESULT_BIT);
	quoted = strdup(((plcMsgQuoteResult *)resp)->result);
	ret = PyString_FromString(quoted);
	pfree(quoted);

	return ret;
}
