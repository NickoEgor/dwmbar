#!/bin/bash

source $XDG_CONFIG_HOME/dwmbar/config.sh

declare -a keys
declare -A commands signals cache

OLDIFS=$IFS
IFS=$'\n'
sig=1
for line in $(cat $SETUP) ; do
    IFS=' ' read -r key cmd <<<$line
    keys+=($key)
    commands[$key]=$(eval echo "$cmd")
    signals[$key]="$sig"
    sig=$(($sig+1))
done
IFS=$OLDIFS

updateValue() {
    cache[$1]="`${commands[$1]}`"
}

updateCache() {
    for key in ${keys[@]} ; do
        updateValue "$key"
    done
}

setBar() {
    statusbar=${cache[${keys[0]}]}
    for key in ${keys[@]:1} ; do
        statusbar=${statusbar}${DELIMITER}${cache[$key]}
    done
	xsetroot -name "$statusbar"
}

setSignals() {
    for key in ${keys[@]} ; do
        trap 'updateValue '"$key"' ; setBar' RTMIN+${signals[$key]}
    done
}

# NOTE: may be troubles with background processes
script_path=$(readlink -f "$0")
trap "exec \"$script_path\"" USR2

# full bar update
trap 'updateCache ; setBar' USR1

setSignals

while true ; do
    updateCache
    setBar

    sleep $DELAY &
    sleep_pid=$!
    wait $sleep_pid || kill $sleep_pid
done
