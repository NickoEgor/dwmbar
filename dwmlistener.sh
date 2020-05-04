#!/bin/bash

source $XDG_CONFIG_HOME/dwmbar/config.sh

declare -A signals

OLDIFS=$IFS
IFS=$'\n'
sig=1
for line in $(cat $SETUP) ; do
    IFS=' ' read -r key <<<$(echo $line | tr -s ' ' | cut -d' ' -f1)
    signals[$key]="$sig"
    sig=$(($sig+1))
done
IFS=$OLDIFS

sendSignal() {
    pkill -RTMIN+${signals["$1"]} dwmbar
}

listenACPI() {
    acpi_listen | while read -r line ; do
        case "$line" in
            button/mute*)           sendSignal "volume" ;;
            button/volumeup*)       sendSignal "volume" ;;
            button/volumedown*)     sendSignal "volume" ;;
            video/brightnessup*)    sendSignal "bright" ;;
            video/brightnessdown*)  sendSignal "bright" ;;
            ac_adapter*)            sendSignal "battery" ;;
            battery*)               sendSignal "battery" ;;
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