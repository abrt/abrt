#!/bin/bash

smart_quote="Program was bored so exited"

function echo_fail {
    echo "*** ERROR: " $1 " ***"
    exit 1
}

if [ -z $1 ]; then
    echo_fail "Gimme file, will ya?"
else
    crash_report=$1
fi

sed -i "s/# Describe the circumstances of this crash below/$smart_quote/" $crash_report && {
    echo "*** File $crash_report edited successfully. ***"
    exit 0
} || echo_fail "Program $0 failed to edit $crash_report. ***"

