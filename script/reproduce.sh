#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 /tmp/jupiter-server-[FILE MD5]" 1>&2
    exit 1
fi

EDIT_DIR=.
EDIT_NAME='SERVER'

# Set script dir
if [ -h $0 ]; then
    EXE=$(readlink $0)
else
    EXE=$0
fi

# Set coeditor dir
EXE_DIR=$(dirname $(realpath $EXE))/..
EXE=${EXE_DIR}/coeditor

# Set debug file prefix
PREFIX=$EDIT_DIR/.coeditor.${EDIT_NAME}.

# make
cd $EXE_DIR
make
cd -

clear

# remove all relating files
rm -f ${PREFIX}*

# new terminal to run the instance
gnome-terminal --title='SERVER' -e "$EXE -od SERVER"

# wait for creating relating file
sleep 0.1
clear

# Set file names
LOCAL_OP_IN=$(ls ${PREFIX}*.local.op.input)
DEBUG_FILE=$(ls ${PREFIX}*.debug)
EXE_PID=${DEBUG_FILE#${PREFIX}}
EXE_PID=${EXE_PID%.debug}

tail --pid=$EXE_PID -c +197 -f $1 > ${LOCAL_OP_IN} &
tail --pid=$EXE_PID -f ${DEBUG_FILE} &

sleep 1
rm -f ${PREFIX}${EXE_PID}*

trap "kill -INT $EXE_PID" 2 15
wait
