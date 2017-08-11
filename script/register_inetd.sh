#!/bin/bash

set -e

if [ "$USERNAME" != "root" ]; then
    echo Please run as root
    exit 1
fi

read -p "Please input a normal username: " INPUT_UNAME

NORM_USER=$(cut -d: -f1 /etc/passwd | grep ${INPUT_UNAME} | head -1)

if [ -z "${NORM_USER}" ]; then
    echo No such user: ${INPUT_UNAME}
    exit 1
fi

read -p "Use \"$NORM_USER\" [y/n] ? " CONFIRM

if [ "$CONFIRM" != "y" ]; then
    echo Cancelled
    exit 1
fi

# Set script dir
if [ -h $0 ]; then
    EXE_DIR=$(readlink $0)
else
    EXE_DIR=$0
fi

EXE_DIR=$(dirname $(realpath $EXE_DIR))/..
EXE=$(ls ${EXE_DIR}/jupiter-server)
INETD_FILE=/etc/inetd.conf
SERVICE_FILE=/etc/services
XINETD_FILE=/etc/xinetd.d/jupiter

INETD=$(echo -e "jupiter\t\tstream\ttcp\tnowait\t${NORM_USER}\t$(realpath ${EXE})\t$(realpath ${EXE})")
SERVICE=$(echo -e "jupiter\t\t$((78663&0xffff))/tcp\t\t\t# Jupiter service")
XINETD=$(echo -e "service jupiter\n{\n\tdisable\t\t= no\n\tsocket_type\t= stream\n\tprotocol\t= tcp\n\tuser\t\t= ${NORM_USER}\n\twait\t\t= no\n\tserver\t\t= $(realpath $EXE)\n\tonly_from\t= 127.0.0.1\n}")

echo
if grep jupiter $SERVICE_FILE &> /dev/null; then
    sed -i 's~^jupiter.*~'"$SERVICE~g" "$SERVICE_FILE"
    echo Updated \'$SERVICE_FILE\':
else
    echo "$SERVICE" >> "$SERVICE_FILE"
    echo Appended to \'$SERVICE_FILE\':
fi
echo "$SERVICE"

echo
if which xinetd &> /dev/null; then
    echo "$XINETD" > $XINETD_FILE
    echo Updated \'$XINETD_FILE\':
    echo "$XINETD"
    killall -HUP xinetd
elif which inetd &> /dev/null; then
    if grep jupiter $INETD_FILE &> /dev/null; then
        sed -i 's~^jupiter.*~'"$INETD~g" "$INETD_FILE"
        echo Updated \'$INETD_FILE\':
    else
        echo "$INETD" >> "$INETD_FILE"
        echo Appended to \'$INETD_FILE\':
    fi
    echo "$INETD"
    killall -HUP inetd
else
    echo "inetd/xinetd not installed"
    exit 1
fi

