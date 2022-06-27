/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_RLOGGING_H
#define PLC_RLOGGING_H

#include <R.h>

SEXP plr_debug(SEXP args);

SEXP plr_log(SEXP args);

SEXP plr_info(SEXP args);

SEXP plr_notice(SEXP args);

SEXP plr_warning(SEXP args);

SEXP plr_error(SEXP args);

SEXP plr_fatal(SEXP args);

#endif /* PLC_RLOGGING_H */
