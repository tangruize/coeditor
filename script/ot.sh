#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e
TEMP=`getopt -o c:t:wp -n $0 -- "$@"`
if [ $? != 0 ]; then echo "Terminating..." >&2; exit 1; fi
eval set -- "$TEMP"

SLEEP_TIME=
SERVER_ADDR=" -c localhost"
COEDITOR_ARGS=

while true; do
    case "$1" in
        -c) SERVER_ADDR=" -c $2 "; shift 2;;
        -t) SLEEP_TIME="-t $2"; shift 2;;
        -w) COEDITOR_ARGS="$COEDITOR_ARGS -w "; shift;;
        -p) COEDITOR_ARGS="$COEDITOR_ARGS -p "; shift;;
        --) shift; break;;
        *)  echo "Internal error!" >&2; exit 1;;
    esac
done

COEDITOR_ARGS="$COEDITOR_ARGS $SERVER_ADDR"

# Check argument num
if [ $# -lt 1 ]; then
    EDIT_DIR=.
    EDIT_NAME='new-buf'
else
    # Set edit file dir
    EDIT_DIR=$(dirname $(realpath $1))
    EDIT_NAME=$(basename $1)
    COEDITOR_ARGS="$COEDITOR_ARGS $1"
fi

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
rm -f ${PREFIX}*.debug ${PREFIX}*.cli ${PREFIX}*.local.op* ${PREFIX}*.server* ${EDIT_DIR}/.ot.*

# new terminal to run the instance
gnome-terminal --title='client 1' -e "$EXE $COEDITOR_ARGS -old"
gnome-terminal --title='client 2' -e "$EXE $COEDITOR_ARGS -old"
#gnome-terminal --title='client 1' -e "gdb $EXE"

# wait for creating debug file
sleep 1
#read -p "OK?" ok

# Set file names
DEBUG_FILE1=$(ls ${PREFIX}*.debug | head -1)
DEBUG_FILE2=$(ls ${PREFIX}*.debug | tail -1)
LOCAL_OP1=$(ls ${PREFIX}*.local.op.output | head -1)
LOCAL_OP2=$(ls ${PREFIX}*.local.op.output | tail -1)
LOCAL_OP_IN1=$(ls ${PREFIX}*.local.op.input | head -1)
LOCAL_OP_IN2=$(ls ${PREFIX}*.local.op.input | tail -1)
LOCAL_OP_IN_F1=$(ls ${PREFIX}*.local.op.input.feedback | head -1)
LOCAL_OP_IN_F2=$(ls ${PREFIX}*.local.op.input.feedback | tail -1)
SERVER_OP1=$(ls ${PREFIX}*.server.output | head -1)
SERVER_OP2=$(ls ${PREFIX}*.server.output | tail -1)
SERVER_OP_IN1=$(ls ${PREFIX}*.server.input | head -1)
SERVER_OP_IN2=$(ls ${PREFIX}*.server.input | tail -1)
SERVER_OP_IN_F1=$(ls ${PREFIX}*.server.input.feedback | head -1)
SERVER_OP_IN_F2=$(ls ${PREFIX}*.server.input.feedback | tail -1)

clear

# monitor debug file
EXE_PID1=${DEBUG_FILE1#$PREFIX}
EXE_PID1=${EXE_PID1%.debug}

EXE_PID2=${DEBUG_FILE2#$PREFIX}
EXE_PID2=${EXE_PID2%.debug}

if [ "$EXE_PID1" = "$EXE_PID2" ]; then
    echo Failed to open two coeditors
    exit 1
fi

${EXE_DIR}/jupiter-ot 0< ${LOCAL_OP1} 1>| ${LOCAL_OP_IN1} 3< ${SERVER_OP1} 4>| ${SERVER_OP_IN1} 5< ${LOCAL_OP_IN_F1} 6< ${SERVER_OP_IN_F1} 2> ${EDIT_DIR}/.ot.${EXE_PID1} $SLEEP_TIME &

${EXE_DIR}/jupiter-ot 0< ${LOCAL_OP2} 1>| ${LOCAL_OP_IN2} 3< ${SERVER_OP2} 4>| ${SERVER_OP_IN2} 5< ${LOCAL_OP_IN_F2} 6< ${SERVER_OP_IN_F2} 2> ${EDIT_DIR}/.ot.${EXE_PID2} $SLEEP_TIME &

echo -e "client 1: $EXE_PID1\n\033[31mclient 2: $EXE_PID2\033[0m"

gnome-terminal --title='debug 1' -e "tail --pid=$EXE_PID1 -f $DEBUG_FILE1 "

gnome-terminal --title='debug 2' -e "tail --pid=$EXE_PID2 -f $DEBUG_FILE2 "

tail --pid=${EXE_PID2} -f ${EDIT_DIR}/.ot.${EXE_PID1} && kill -- -$$ &
tail --pid=${EXE_PID1} -f ${EDIT_DIR}/.ot.${EXE_PID2} | sed -e 's/^/\x1b[31m/' -e 's/$/\x1b[0m/' && kill -- -$$
