#!/bin/bash

if [ "$USERNAME" != "root"  ]; then
    echo Please run as root
    exit 1
fi

if which inetd &> /dev/null; then
    sed -i '/^jupiter.*/d' /etc/inetd.conf
    killall -HUP inetd
elif which xinetd &> /dev/null; then
    rm -f /etc/xinetd.d/jupiter
    killall -HUP xinetd
fi

