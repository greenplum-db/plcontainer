/**
 * SQL message handler implementation.
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "executor/spi.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "access/xact.h"

#include "common/comm_channel.h"
#include "subtransaction_handler.h"

/* list of explicit subtransaction data */
List *explicit_subtransactions;

/* explicit subtransaction data */
typedef struct PLySubtransactionData {
	MemoryContext oldcontext;
	ResourceOwner oldowner;
} PLySubtransactionData;



static int16 plcontainer_subtransaction_enter();

static int16 plcontainer_subtransaction_exit(plcMsgSubtransaction *msg);


/*
 * Enter a subtransaction
 *
 * Start an explicit subtransaction.  SPI calls within an explicit
 * subtransaction will not start another one, so you can atomically
 * execute many SPI calls and still get a controllable exception if
 * one of them fails.
 */
static int16
plcontainer_subtransaction_enter() {
	PLySubtransactionData* volatile subxactdata = NULL;	
	MemoryContext oldcontext;
	
	plc_elog(DEBUG1, "subtransaction enter beigin");
	oldcontext = CurrentMemoryContext;

	PG_TRY();
	{
		subxactdata = plc_top_alloc(sizeof(*subxactdata));
		subxactdata->oldcontext = oldcontext;
		subxactdata->oldowner = CurrentResourceOwner;

		BeginInternalSubTransaction(NULL);
	}
	PG_CATCH();
	{
		if (subxactdata != NULL) {
			pfree(subxactdata);
			subxactdata = NULL;
		}
		/* If enter subtransaction failed, we should return error to python code and
		 * let python code to handle the error. Don't directly throw error in QE side
		 */
		return CREATE_SUBTRANSACTION_ERROR;
	}
	PG_END_TRY();

	/* Do not want to leave the previous memory context */
	MemoryContextSwitchTo(oldcontext);

	explicit_subtransactions = lcons(subxactdata, explicit_subtransactions);
	plc_elog(DEBUG1, "subtransaction enter end, current explicit transaction num:%d",
	     list_length(explicit_subtransactions));
	return SUCCESS;
}

/*
 *  Exit a subtransaction. 
 *  msg type indicate the exception type. 'e' mean there is no exception.
 *  When exit with exception, we need to rollback.
 */
static int16 plcontainer_subtransaction_exit(plcMsgSubtransaction *msg) {
	PLySubtransactionData *subxactdata;

	plc_elog(DEBUG1, "subtransaction exit begin");	
	if (explicit_subtransactions == NIL) {
		return NO_SUBTRANSACTION_ERROR;
	}
	PG_TRY();
	{
		/* message type is not 'e' means that we need to rollback */
		if (msg->type != 'e') {
			/* Abort the inner transaction */
			RollbackAndReleaseCurrentSubTransaction();
		} else {
			ReleaseCurrentSubTransaction();
		}
	}
	PG_CATCH();
	{
		return RELEASE_SUBTRANSACTION_ERROR;
	}
	PG_END_TRY();
	subxactdata = (PLySubtransactionData *) linitial(explicit_subtransactions);
	explicit_subtransactions = list_delete_first(explicit_subtransactions);

	MemoryContextSwitchTo(subxactdata->oldcontext);
	CurrentResourceOwner = subxactdata->oldowner;
	pfree(subxactdata);

	/*
	 * AtEOSubXact_SPI() should not have popped any SPI context, but just in
	 * case it did, make sure we remain connected.
	 */
	SPI_restore_connection();
	plc_elog(DEBUG1, "subtransaction exit end, current explicit transaction num: %d",
	     list_length(explicit_subtransactions));
	return SUCCESS;
}

void plcontainer_process_subtransaction(plcMsgSubtransaction *msg, plcConn *conn) {
	int16 res = 0;
	plcMsgSubtransactionResult *result;
	result = palloc(sizeof(plcMsgSubtransactionResult));
	result->msgtype = MT_SUBTRAN_RESULT;

	/*operation == 'n' means enter, 'x' means exit subtransaction*/
	if (msg->action == 'n') {
		res = plcontainer_subtransaction_enter();
	} else if (msg->action == 'x') {
		res = plcontainer_subtransaction_exit(msg);
	}
	result->result = res;

	res = plcontainer_channel_send(conn, (plcMessage *) result);
	if (res < 0) {
		plc_elog(ERROR, "Error sending data to the client, with errno %d. ", res);
	}
}

/*
 * Abort lingering subtransactions that have been explicitly started
 * by plpy.subtransaction().start() and not properly closed.
 */
void
plcontainer_abort_open_subtransactions(int save_subxact_level) {
	Assert(save_subxact_level >= 0);
	plc_elog(DEBUG1, "explicit_subtransactions length %d:%d", list_length(explicit_subtransactions), save_subxact_level);
	while (list_length(explicit_subtransactions) > save_subxact_level) {
		PLySubtransactionData *subtransactiondata;

		Assert(explicit_subtransactions != NIL);

		ereport(WARNING,
		        (errmsg("forcibly aborting a subtransaction that has not been exited")));

		RollbackAndReleaseCurrentSubTransaction();

		SPI_restore_connection();

		subtransactiondata = (PLySubtransactionData *) linitial(explicit_subtransactions);
		explicit_subtransactions = list_delete_first(explicit_subtransactions);

		MemoryContextSwitchTo(subtransactiondata->oldcontext);
		CurrentResourceOwner = subtransactiondata->oldowner;
		pfree(subtransactiondata);
	}
}

