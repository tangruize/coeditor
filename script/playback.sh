#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

EDIT_DIR=.
EDIT_NAME='new-buf'

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
rm -f ${PREFIX}*.local.op*

# new terminal to run the instance
gnome-terminal --title='coeditor' -e "$EXE"

# wait for creating relating file
sleep 1
clear

# Set file names
LOCAL_OP_IN=$(ls ${PREFIX}*.local.op.input)

for i in $(seq 0 $(($(stat -c %s ${EXE_DIR}/local_ops) / 16 - 1))); do
    if [ ! -p $LOCAL_OP_IN ]; then
        exit
    fi
    dd if=${EXE_DIR}/local_ops of=${LOCAL_OP_IN} count=16 \
        skip=$((i * 16)) iflag=count_bytes,skip_bytes 2> /dev/null
    sleep 0.15
done
