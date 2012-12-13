#!/bin/sh

stopfile="/var/spool/abrt/bogofile.stop.$$"
>"$stopfile"

f() {
    name="/var/spool/abrt/bogofile.$RANDOM"
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
