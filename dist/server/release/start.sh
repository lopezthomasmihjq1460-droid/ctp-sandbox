#!/bin/bash
SOCK=/tmp/ctp_sandbox.sock
BIN=./ctp_sandbox.exe

start() {
    if [ -S $SOCK ];then
        echo "server already running"
        return
    fi
    $BIN -d
    echo "server daemon started"
}

stop() {
    if [ ! -S $SOCK ];then
        echo "server not running"
        return
    fi
    echo "shutdown" | socat - UNIX-CONNECT:$SOCK
    sleep 0.3
    rm -f $SOCK
    echo "server stopped gracefully"
}

restart() {
    stop
    sleep 0.5
    start
}

case $1 in
start) start;;
stop) stop;;
restart) restart;;
*) echo "usage: $0 start|stop|restart";;
esac
