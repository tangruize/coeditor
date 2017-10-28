#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e
TEMP=`getopt -o c:t:T:n:a:swp -n $0 -- "$@"`
if [ $? != 0 ]; then echo "Terminating..." >&2; exit 1; fi
eval set -- "$TEMP"

SLEEP_TIME=
RECV_TIME=
SERVER_ADDR=" -c localhost"
COEDITOR_ARGS=
GEN_NUM="1000"

while true; do
    case "$1" in
        -c) SERVER_ADDR=" -c $2 "; shift 2;;
        -t) SLEEP_TIME="-t $2"; shift 2;;
        -T) RECV_TIME="-T $2"; shift 2;;
        -n) GEN_NUM="$2"; shift 2;;
        -a) COEDITOR_ARGS="$COEDITOR_ARGS -a $2 "; shift 2;;
        -w) COEDITOR_ARGS="$COEDITOR_ARGS -w "; shift;;
        -p) COEDITOR_ARGS="$COEDITOR_ARGS -p "; shift;;
        -s) COEDITOR_ARGS="$COEDITOR_ARGS -s "; shift;;
        --) shift; break;;
        *)  echo "Internal error!" >&2; exit 1;;
    esac
done

if [ $# -lt 1 ]; then
    NUM=2;
else
    NUM=$1
fi

if [ $NUM -gt 32 ]; then
    NUM=32
fi

for i in `seq 0 $((NUM-1))`; do
    EDIT_NAME[i]=$RANDOM
done

EDIT_DIR=.

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
PREFIX=$EDIT_DIR/.coeditor

# make
cd $EXE_DIR
make
cd -

clear
k=1
PIDS[0]=$$
for i in ${EDIT_NAME[@]}; do
    INVOKE_COEDITOR_ARGS="$COEDITOR_ARGS $SERVER_ADDR $i"
    gnome-terminal --title="client $k" -e "$EXE $INVOKE_COEDITOR_ARGS -ld"
    TRY_TIMES=10
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.debug &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    DEBUG_FILE=$(ls ${PREFIX}.${i}*.debug)
    EXE_PID=${DEBUG_FILE#$PREFIX.${i}.}
    EXE_PID=${EXE_PID%.debug}
    PIDS[$((k-1))]=$EXE_PID
    echo -e "\033[$((k+30))mclient $k: $EXE_PID\033[0m"
    gnome-terminal --title="debug $k" -e "tail --pid=$EXE_PID -f $DEBUG_FILE "
    k=$((k+1))
done

trap "kill -INT ${PIDS[*]} $SERVER_PID &>/dev/null;trap "" 2 15;kill -- -$$ &> /dev/null" 2 15

read -p "Ready? " ok
for i in ${PIDS[@]}; do
    CLI_FILE=$(ls ${PREFIX}*${i}*.cli)
    ${EXE_DIR}/op-generator $GEN_NUM > $CLI_FILE &
done

$EXE -S

kill $$
