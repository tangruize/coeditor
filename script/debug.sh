#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# Check argument num
if [ $# -lt 1 ]; then
    EDIT_DIR=.
    EDIT_NAME='new-buf'
else
    # Set edit file dir
    EDIT_DIR=$(dirname $(realpath $1))
    EDIT_NAME=$(basename $1)
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

# make exit successfully, clear screen
clear
echo "Usage:"
echo "  I offset data"
echo "  D offset"
echo "  i row col data"
echo "  d row col"
echo "  s filename"
echo "  p [line [count]]"

# remove all relating files
rm -f ${PREFIX}*.debug ${PREFIX}*.cli ${PREFIX}*.local.op* ${PREFIX}*.server* ${EXE_DIR}/local_ops ${EXE_DIR}/server_ops

# new terminal to run the instance
gnome-terminal --title='coeditor' -e "$EXE $*"

# wait for creating debug file
sleep 3
clear

# Set file names
set +e
DEBUG_FILE=$(ls ${PREFIX}*.debug)
CLI_FILE=$(ls ${PREFIX}*.cli)
LOCAL_OP=$(ls ${PREFIX}*.local.op.output)
LOCAL_OP_IN=$(ls ${PREFIX}*.local.op.input)
SERVER_OP=$(ls ${PREFIX}*.server.output)
SERVER_OP_IN=$(ls ${PREFIX}*.server.input)
set -e

NDEBUG=0
clear

if [ ! -f "$DEBUG_FILE" ]; then
    # No debug file
    echo debug disabled
    NDEBUG=1
else
    # monitor debug file
    EXE_PID=${DEBUG_FILE#$PREFIX}
    EXE_PID=${EXE_PID%.debug}
    tail --pid=$EXE_PID -f $DEBUG_FILE && kill -- -$$ &
fi

if [ -p "$SERVER_OP_IN" ]; then
    LO_REDIRECT="tee $SERVER_OP_IN | xxd -c 12"
else
    LO_REDIRECT="xxd -c 12"
fi

if [ -p "$LOCAL_OP" ]; then
    # monitor local op
    if [ $NDEBUG -eq 0 ]; then
        gnome-terminal --title='local operations' \
            -e "bash -c \"cat $LOCAL_OP | tee $EXE_DIR/local_ops | $LO_REDIRECT\""
    else
        cat $LOCAL_OP | tee $EXE_DIR/local_ops | $LO_REDIRECT
    fi
fi

if [ -p "$SERVER_OP" ]; then
    gnome-terminal --title='from server' \
        -e "bash -c \"cat $SERVER_OP | tee $EXE_DIR/server_ops | xxd -c 20\""
fi

if [ -p "$CLI_FILE" ]; then
    # write cli file
    cat > $CLI_FILE
fi
