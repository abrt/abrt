#!/bin/bash

function check_prior_crashes() {
    rlAssert0 "No prior crashes recorded" $(abrt-cli list 2> /dev/null | wc -l)
    if [ ! "_$(abrt-cli list 2> /dev/null | wc -l)" == "_0" ]; then
        abrt-cli list
        rlDie "Won't proceed"
    fi
}

function get_crash_path() {
    rlLog "Get crash path"
    rlAssertGreater "Crash recorded" $(abrt-cli list 2> /dev/null | wc -l) 0
    crash_PATH="$(abrt-cli list 2> /dev/null | grep Directory | awk '{ print $2 }' | tail -n1)"
    crash_PATH_RIGHTS=$(ls -la $crash_PATH | grep ' .$' | awk '{print $1}')
    crash_PATH_USER=$(ls -la $crash_PATH | grep ' .$' | awk '{print $3}')
    crash_PATH_GROUP=$(ls -la $crash_PATH | grep ' .$' | awk '{print $4}')
    if [ ! -d "$crash_PATH" ]; then
        rlDie "No crash dir generated, this shouldn't happen"
    fi
    rlLog "PATH = $crash_PATH"
}

function wait_for_process() {
    local procname=$1
    local timeout=3000
    if [ $# -gt 1 ]; then
        timeout=$2
    fi
    rlLog "Waiting for $procname to end"
    local c=0
    while pidof -x "$procname" &> /dev/null; do
        sleep 0.1
        let c=$c+1
        if [ $c -gt $timeout ]; then
            rlFail "Timeout"
            break
        fi
    done
    t=$( echo "scale=2; $c/10" | bc )
    rlLog "Process ended in $t seconds"
}

function wait_for_sosreport() {
    wait_for_process sosreport
}

function wait_for_hooks() {
    rlLog "Waiting for all hooks to end"
    local c=0
    while [ ! -f "/tmp/abrt-done" ]; do
        sleep 0.1
        let c=$c+1
        if [ $c -gt 3000 ]; then
            rlFail "Timeout"
            break
        fi
    done
    t=$( echo "scale=2; $c/10" | bc )
    rlLog "Hooks ended in $t seconds"
}

function generate_crash() {
    rlLog "Generate crash"
    su -c will_segfault $1
}

function generate_second_crash() {
    rlLog "Generate second crash"
    su -c will_abort $1
}

function generate_stack_overflow_crash() {
    rlLog "Generate stack overflow"
    su -c will_stackoverflow $1
}

function generate_python_segfault() {
    rlLog "Generate python segfault"
    su -c will_python_sigsegv $1
}

function generate_python_exception() {
    rlLog "Generate unhandled python exception"
    su -c will_python_raise $1
}

function generate_python3_exception() {
    rlLog "Generate unhandled python3 exception"
    su -c will_python3_raise $1
}

function load_abrt_conf() {
    ABRT_CONF_DUMP_LOCATION=/var/tmp/abrt/

    if test -z "$ABRT_CONF_DUMP_LOCATION"; then
        ABRT_CONF_DUMP_LOCATION=/var/tmp/abrt/
    fi

    if test -z "$ABRT_CONF_DUMP_LOCATION"; then
        ABRT_CONF_DUMP_LOCATION=/var/tmp/abrt/
    fi
}

function prepare() {
    load_abrt_conf

    rm -f -- $ABRT_CONF_DUMP_LOCATION/last-ccpp
    rm -f "/tmp/abrt-done"
}

# should be faster than "sleep 1"
function wait_for_server() {
    PORT=$1

    for I in `seq 100`; do
        if nc -4 localhost $PORT </dev/null &>/dev/null; then
            break
        fi
        sleep 0.01
    done
}
