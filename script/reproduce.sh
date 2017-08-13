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
TRY_TIMES=30
while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}*.local.op.input &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}*.debug &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
clear
if [ $TRY_TIMES -eq 0 ]; then
    echo timeout: pipe files not detected for server
    exit 1
fi

# Set file names
LOCAL_OP_IN=$(ls ${PREFIX}*.local.op.input)
DEBUG_FILE=$(ls ${PREFIX}*.debug)
EXE_PID=${DEBUG_FILE#${PREFIX}}
EXE_PID=${EXE_PID%.debug}

tail --pid=$EXE_PID -c +197 -f $1 | dd of="${LOCAL_OP_IN}" bs=12 &> /dev/null &
tail --pid=$EXE_PID -f ${DEBUG_FILE} &

sleep 1
rm -f ${PREFIX}${EXE_PID}*

trap "trap '' 2 15;kill -INT $EXE_PID" 2 15
wait
