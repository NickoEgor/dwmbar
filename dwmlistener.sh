#!/bin/bash
# vim: ft=bash

sendSignal() {
    echo "$1 0" > /tmp/dwmbar.fifo
}

listenACPI() {
    acpi_listen | while read -r line ; do
        sleep 0.1
        case "$line" in
            button/mute*)      sendSignal "volume" ;;
            button/volume*)    sendSignal "volume" ;;
            video/brightness*) sendSignal "bright" ;;
            ac_adapter*)       sendSignal "battery" ;;
            battery*)          sendSignal "battery" ;;
        esac
    done
}

listenPlayer() {
    while :; do
        mpc idle >/dev/null && sendSignal "player"
    done
}

listenXKB() {
    while :; do
        xkb-switch -w && sendSignal "keyboard"
    done
}

kill_descendant_processes() {
    local pid="$1"
    local and_self="${2:-false}"
    if children="$(pgrep -P "$pid")"; then
        for child in $children; do
            kill_descendant_processes "$child" true
        done
    fi
    if [[ "$and_self" == true ]]; then
        kill "$pid"
    fi
}

listenXKB &
listenPlayer &
listenACPI &

trap "kill_descendant_processes $$" INT
trap "kill_descendant_processes $$" TERM
trap "kill_descendant_processes $$" HUP

script_path=$(readlink -f "$0")
trap "kill_descendant_processes $$ ; exec \"$script_path\"" USR1

exec sleep infinity
