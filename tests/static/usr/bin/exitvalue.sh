#! /bin/bash
#
# exitvalue.sh

function _exit {
	echo "exiting ($1)"
	exit $1
}

if [ $# != 1 ]; then
	_exit 1
fi

test $1 -eq $1 2> /dev/null
if [ $? -ne 0 ]; then
	_exit 1
elif [ $1 -lt 0 -o $1 -gt 255 ]; then
	_exit 1
fi
_exit $1
