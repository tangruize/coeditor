#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e
TEMP=`getopt -o c:t:T:wpfF -n $0 -- "$@"`
if [ $? != 0 ]; then echo "Terminating..." >&2; exit 1; fi
eval set -- "$TEMP"

SLEEP_TIME=
RECV_TIME=
SERVER_ADDR=" -c localhost"
COEDITOR_ARGS=" -F "

while true; do
    case "$1" in
        -c) SERVER_ADDR=" -c $2 "; shift 2;;
        -t) SLEEP_TIME="-t $2"; shift 2;;
        -T) RECV_TIME="-T $2"; shift 2;;
        -w) COEDITOR_ARGS="$COEDITOR_ARGS -w "; shift;;
        -p) COEDITOR_ARGS="$COEDITOR_ARGS -p "; shift;;
        -f) COEDITOR_ARGS="$COEDITOR_ARGS -f "; shift;;
        -F) COEDITOR_ARGS="$COEDITOR_ARGS -F "; shift;;
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
    gnome-terminal --title="client $k" -e "$EXE $INVOKE_COEDITOR_ARGS -od"
    TRY_TIMES=30
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.server.input.feedback &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.server.input &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.server.output &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.local.op.input.feedback &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.local.op.input &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.local.op.output &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${PREFIX}.${i}*.debug &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    if [ $TRY_TIMES -eq 0 ]; then
        echo timeout: pipe files not detected for client $k
        kill -INT ${PIDS[*]} &>/dev/null
        kill -- -$$ &> /dev/null
        exit 1
    fi
    DEBUG_FILE=$(ls ${PREFIX}.${i}*.debug)
    LOCAL_OP=$(ls ${PREFIX}.${i}*.local.op.output)
    LOCAL_OP_IN=$(ls ${PREFIX}.${i}*.local.op.input)
    LOCAL_OP_IN_F=$(ls ${PREFIX}.${i}*.local.op.input.feedback)
    SERVER_OP=$(ls ${PREFIX}.${i}*.server.output)
    SERVER_OP_IN=$(ls ${PREFIX}.${i}*.server.input)
    SERVER_OP_IN_F=$(ls ${PREFIX}.${i}*.server.input.feedback)
    EXE_PID=${DEBUG_FILE#$PREFIX.${i}.}
    EXE_PID=${EXE_PID%.debug}
    PIDS[$((k-1))]=$EXE_PID
    echo -e "\033[$((k+30))mclient $k: $EXE_PID\033[0m"
    ${EXE_DIR}/jupiter-ot 0< ${LOCAL_OP} 1>| ${LOCAL_OP_IN} 3< ${SERVER_OP} 4>| ${SERVER_OP_IN} 5< ${LOCAL_OP_IN_F} 6< ${SERVER_OP_IN_F} 2> ${EDIT_DIR}/.ot.${i} $SLEEP_TIME $RECV_TIME &
#    rm -f ${LOCAL_OP} ${LOCAL_OP_IN} ${LOCAL_OP_IN_F} ${SERVER_OP} ${SERVER_OP_IN} ${SERVER_OP_IN_F}
    while [ $TRY_TIMES -ne 0 ] && ! eval ls ${EDIT_DIR}/.ot.${i} &> /dev/null; do sleep 0.1; TRY_TIMES=$((TRY_TIMES-1)); done
    tail --pid=${EXE_PID} -f ${EDIT_DIR}/.ot.${i} | sed -e "s/^/\x1b[$((k+30))m/" -e 's/$/\x1b[0m/' && kill -- -$$ &
    gnome-terminal --title="debug $k" -e "tail --pid=$EXE_PID -f $DEBUG_FILE "
    k=$((k+1))
done

sleep 1
rm -f ${PREFIX}.* ${EDIT_DIR}/.ot.*
${EXE_DIR}/script/reproduce.sh /tmp/jupiter-server-d41d8cd98f00b204e9800998ecf8427e > /dev/null &

trap "kill -INT ${PIDS[*]} $SERVER_PID &>/dev/null;trap "" 2 15;kill -- -$$ &> /dev/null" 2 15

wait
