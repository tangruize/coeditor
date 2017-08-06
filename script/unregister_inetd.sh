#!/bin/bash

if [ "$USERNAME" != "root"  ]; then
    echo Please run as root
    exit 1
fi

sed -i '/^jupiter.*/d' /etc/inetd.conf

killall -HUP inetd
