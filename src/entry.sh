#!/usr/bin/env sh
# shellcheck disable=SC2268
# SC2268 = xprefix
# POSIX shell only. this file runs in ash/dash/bash
# ------------------------------------------------------------------------------
#
# Copyright (c) 2016-Present Pivotal Software, Inc
#
# ------------------------------------------------------------------------------

# file struct inside container:
# /clientdir
#   -> pyclient.sh   (hardcopy of entry.sh. exec(2) does not support symlink)
#   -> py3client.sh  (hardcopy of entry.sh)
#   -> rclient.sh    (hardcopy of entry.sh)
#   -> pyclient      (the default one, symlink to client_python27_ubuntu-18.04)
#   -> py3client     (the default one, symlink to client_python39_ubuntu-22.04)
#   -> rclient       (the default one, symlink to client_r3.6.3_ubuntu-18.04  )
#   -> client_python27_ubuntu-18.04
#   -> client_python39_ubuntu_22.04
#   -> client_r3.6.3_ubuntu-18.04
#   -> client_<client_name>_<OS_ID>-<BUILD_ID or VERSION_ID or image hash>
#   -> source_python_client
#   -> source_python3_client (symlink to source_python_client)
#   -> source_r_client
#
# example usage:
#   -l <_> client = <empty> : error
#   -l py2 client = <empty> : exec '/clientdir/pyclient.sh'   no build
#   -l py3 client = <empty> : exec '/clientdir/py3client.sh'  no build
#   -l r   client = <empty> : exec '/clientdir/rclient.sh'    no build
#   -l py3 client = python31: exec '/clientdir/client_python31_<OS>'. build 'source_python3_client'            OK
#   -l py  client = python31: exec '/clientdir/client_python31_<OS>'. build 'source_python_client' , exec py31 OK
#   -l r   client = r4.3.0  : exec '/clientdir/client_r4.3.0_<OS>'.   build 'source_r_client'      , exec r    OK
#   -l r   client = python31: exec '/clientdir/client_python31_<OS>'. build 'source_r_client'      , exec r    PANIC

case "$0" in
	"/clientdir/py3client.sh")
		PLC_LANGUAGE="python3"
		PLC_DEFAULT_CLIENT="/clientdir/py3client"
	;;
	"/clientdir/pyclient.sh")
		PLC_LANGUAGE="python"
		PLC_DEFAULT_CLIENT="/clientdir/pyclient"
	;;
	"/clientdir/rclient.sh")
		PLC_LANGUAGE="r"
		PLC_DEFAULT_CLIENT="/clientdir/rclient"
		;;
	*)
		true
		;;
esac

export LD_LIBRARY_PATH="/clientdir:$LD_LIBRARY_PATH" # rclient need load lib

if [ x"" = x"$PLC_CLIENT" ] && [ x"" != x"$PLC_DEFAULT_CLIENT" ]; then
	# the default client, configure file empty.
	exec $(readlink -f "$PLC_DEFAULT_CLIENT")
	exit $?
fi

if [ x"" = x"$PLC_LANGUAGE" ]; then
	echo "can not detect plc language. make sure the file name is '/clientdir/<XX>client.sh'"
	exit 1
fi

PLC_CLIENT_SUFFFIX="$(hostname)"

if [ -f "/etc/os-release" ]; then
	OS_ID=$(sh -c '. /etc/os-release; echo $ID')
	OS_BUILD_ID=$(sh -c '. /etc/os-release; echo $BUILD_ID')
	OS_VERSION_ID=$(sh -c '. /etc/os-release; echo $VERSION_ID')

	PLC_CLIENT_SUFFFIX="$OS_ID"

	if [ x"" != x"$OS_BUILD_ID" ]; then
		PLC_CLIENT_SUFFFIX="$PLC_CLIENT_SUFFFIX-$OS_BUILD_ID"
	elif [ x"" != x"$OS_VERSION_ID" ]; then
		PLC_CLIENT_SUFFFIX="$PLC_CLIENT_SUFFFIX-$OS_VERSION_ID"
	else
		PLC_CLIENT_SUFFFIX="$PLC_CLIENT_SUFFFIX-$PLC_IMAGE_HASH"
	fi
fi

sh -c "/clientdir/client_$PLC_CLIENT""_""$PLC_CLIENT_SUFFFIX"
r="$?"

if [ "$r" = "127" ]; then # no such file, try to build
	echo "try to execute /clientdir/client_$PLC_CLIENT""_""$PLC_CLIENT_SUFFFIX"" but errored, try to build $PLC_LANGUAGE client" > /dev/stderr
	PLC_BUILD_DIR="/tmp/plc_build"
	mkdir "$PLC_BUILD_DIR"

	# not remove the tempdir. container is not persistent
	cmake "/clientdir/source_""$PLC_LANGUAGE""_client" -B "$PLC_BUILD_DIR"
	make -C "$PLC_BUILD_DIR"

	client="$(find "$PLC_BUILD_DIR/" -name "client*" -type f | grep -v '.dir')"
	chown "$(stat -c '%u:%g' "$0")" -R "$client" # error able
	cp -a "$client" "/clientdir/"                # error able
	exec "$client"
	exit $?
fi

exit "$r"
