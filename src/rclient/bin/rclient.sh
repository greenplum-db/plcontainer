#!/bin/sh
# ------------------------------------------------------------------------------
#
# Copyright (c) 2016-Present Pivotal Software, Inc
#
# ------------------------------------------------------------------------------
cd /clientdir

export LD_LIBRARY_PATH="/clientdir:$LD_LIBRARY_PATH"
./rclient

