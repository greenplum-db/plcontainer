

/**
 * SQL message handler implementation.
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/spi.h"

#include "common/comm_channel.h"
#include "plcontainer_common.h"
#include "subtransaction_handler.h"


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
plcontainer_subtransaction_enter()
{
	elog(DEBUG1, "subtransaction enter beigin");
	PLySubtransactionData *subxactdata = NULL;
	MemoryContext oldcontext;
	oldcontext = CurrentMemoryContext;

	PG_TRY();
	{
		subxactdata = plcontainer_malloc(sizeof(*subxactdata));
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
	elog(DEBUG1, "subtransaction enter end, current explicit transaction num:%d", list_length(explicit_subtransactions));
	return SUCCESS;
}

/*
 *  Exit a subtransaction. 
 *  msg type indicate the exception type. 'e' mean there is no exception.
 *  When exit with exception, we need to rollback.
 */ 
static int16 plcontainer_subtransaction_exit(plcMsgSubtransaction *msg){
	elog(DEBUG1, "subtransaction exit begin");
	PLySubtransactionData *subxactdata;
	if (explicit_subtransactions == NIL)
	{
		return NO_SUBTRANSACTION_ERROR;
	}
	PG_TRY();
	{
		/* message type is not 'e' means that we need to rollback */
		if (msg->type != 'e')
		{
			/* Abort the inner transaction */
			RollbackAndReleaseCurrentSubTransaction();
		}
		else
		{
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
	plcontainer_free(subxactdata);

	/*
	 * AtEOSubXact_SPI() should not have popped any SPI context, but just in
	 * case it did, make sure we remain connected.
	 */
	SPI_restore_connection();
	elog(DEBUG1, "subtransaction exit end, current explicit transaction num: %d", list_length(explicit_subtransactions));
	return SUCCESS;
}

void plcontainer_process_subtransaction(plcMsgSubtransaction *msg, plcConn *conn) {
	int16 res;
	plcMsgSubtransactionResult  *result;
	result          = palloc(sizeof(plcMsgSubtransactionResult));
	result->msgtype = MT_SUBTRAN_RESULT;

	/*operation == 'n' means enter, 'x' means exit subtransaction*/
	if (msg->action == 'n') {
		res = plcontainer_subtransaction_enter();
	} else if (msg->action == 'x') {
		res = plcontainer_subtransaction_exit(msg);
	}
	result->result = res;

	res = plcontainer_channel_send(conn, (plcMessage*)result);
	if (res < 0) {
		elog(ERROR, "Error sending data to the client, with errno %d. ", res);
	}
}


/*
 * Abort lingering subtransactions that have been explicitly started
 * by plpy.subtransaction().start() and not properly closed.
 */
static void
plcontainer_abort_open_subtransactions(int save_subxact_level)
{
	Assert(save_subxact_level >= 0);
	elog(LOG, "explicit_subtransactions length %d:%d", list_length(explicit_subtransactions), save_subxact_level);
	while (list_length(explicit_subtransactions) > save_subxact_level)
	{
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
		plcontainer_free(subtransactiondata);
	}
}/*
 * Abort lingering subtransactions that have been explicitly started
 * by plpy.subtransaction().start() and not properly closed.
 */
void
plcontainer_abort_open_subtransactions(int save_subxact_level)
{
	Assert(save_subxact_level >= 0);
	elog(DEBUG1, "explicit_subtransactions length %d:%d", list_length(explicit_subtransactions), save_subxact_level);
	while (list_length(explicit_subtransactions) > save_subxact_level)
	{
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
		plcontainer_free(subtransactiondata);
	}
}

