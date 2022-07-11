#pragma once

#include "message_base.h"

typedef struct plcMsgAnytable {
	base_message_content;
	Datum anytable;
	int32 method; // TODO QQQ which method? only __next__ ?
				  // client get AnyTable<Datum> from UDF args
				  // client send Datum to server at first time. get TypeInfo[] back
				  // then __next__(), get Datum[] back
} plcMsgAnytable;
