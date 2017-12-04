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
 * file name:			plcontainer_channel.h
 * description:			Channel API.
 * author:			Laszlo Hornyak Kocka
 */

#ifndef PLC_COMM_CHANNEL_H
#define PLC_COMM_CHANNEL_H

#include "comm_connectivity.h"
#include "messages/messages.h"

//#define PLCONTAINER_CHANNEL_DEBUG

#ifdef PLCONTAINER_CHANNEL_DEBUG
	#define debug_print(...) lprintf(__VA_ARGS__)
#else
	#define debug_print(...)
#endif

int plcontainer_channel_send(plcConn *conn, plcMessage *msg);

int plcontainer_channel_receive(plcConn *conn, plcMessage **msg, int64 mask);

void fill_prepare_argument(plcArgument *arg, char *str, plcDatatype plcData);

#endif /* PLC_COMM_CHANNEL_H */
