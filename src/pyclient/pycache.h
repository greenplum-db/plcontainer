/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PYCACHE_H
#define PLC_PYCACHE_H

#include <Python.h>
#include "pyconversions.h"

#define PLC_PY_FUNCTION_CACHE_SIZE 20

plcPyFunction *plc_py_function_cache_get(unsigned int objectid);

void plc_py_function_cache_put(plcPyFunction *func);

#endif /* PLC_PYCACHE_H */
