#!/bin/sh

. ../aux/lib.sh

load_abrt_conf

stopfile="$ABRT_CONF_DUMP_LOCATION/bogofile.stop.$$"
>"$stopfile"

f() {
    name="$ABRT_CONF_DUMP_LOCATION/bogofile.$RANDOM"
    while test -f "$stopfile"; do
        rm "$name" >"$name"
    done
}

f &
f &
f &
f &
sleep 1
rm "$stopfile"
wait
