#!/bin/bash

function check_prior_crashes() {
    rlAssert0 "No prior crashes recorded" $(abrt status --bare)
    if [ "$(abrt status --bare)" -ne 0 ]; then
        abrt list
        rlDie "Won't proceed"
    fi
}

function check_dump_dir_attributes() {
    rlLog "Check dump dir FS attributes"
    if [ -z "$1" ]; then
        rlFail "Missing path to the test dump directory"
        return
    fi

    ls -al $1
    rlAssertEquals "Dump directory has proper rights" "_$(stat --format=%A $1)" "_drwxr-x---"
    rlAssertEquals "Dump directory owned by root" "x$(stat --format=%U $1)" "xroot"
    rlAssertEquals "Dump directory group is abrt" "x$(stat --format=%G $1)" "xabrt"
}

function check_dump_dir_attributes_vmcore_rhel() {
    rlLog "Check dump dir FS attributes"
    if [ -z "$1" ]; then
        rlFail "Missing path to the test dump directory"
        return
    fi

    ls -al $1
    rlAssertEquals "Dump directory has proper rights" "_$(stat --format=%A $1)" "_drwx------"
    rlAssertEquals "Dump directory owned by root" "x$(stat --format=%U $1)" "xroot"
    rlAssertEquals "Dump directory group is abrt" "x$(stat --format=%G $1)" "xroot"
}

function get_first_crash_path() {
    rlLog "Get crash path"
    rlAssertGreater "Crash recorded" $(abrt status --bare) 0
    crash_PATH="$(abrt list --format={path} 2> /dev/null | tail --lines=1)"
    if [ ! -d "$crash_PATH" ]; then
        echo "Dump location listing:"
        ls -l $ABRT_CONF_DUMP_LOCATION
        echo "abrt list:"
        abrt list
        echo "Syslog:"
        print_syslog 10
        rlFail "No crash dir generated, this shouldn't happen"
    fi
    rlLog "PATH = $crash_PATH"
}

function get_last_crash_path() {
    rlLog "Get crash path"
    rlAssertGreater "Crash recorded" $(abrt status --bare) 0
    crash_PATH="$(abrt info --format={path} 2> /dev/null)"
    if [ ! -d "$crash_PATH" ]; then
        echo "Dump location listing:"
        ls -l $ABRT_CONF_DUMP_LOCATION
        echo "abrt list:"
        abrt list
        echo "Syslog:"
        print_syslog 10
        rlFail "No crash dir generated, this shouldn't happen"
    fi
    rlLog "PATH = $crash_PATH"
}

function get_crash_path() {
    get_last_crash_path
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

function wait_for_hooks() {
    rlLog "Waiting for all hooks to end"
    # Wait at least 1 second
    sleep 1
    local c=0
    while [ ! -f "/tmp/abrt-done" ]; do
        sleep 0.1
        let c=$c+1
        if [ $c -gt 3000 ]; then
            rlFail "Timeout"
            break
        fi
    done
    t=$( echo "scale=2; ($c/10)+1" | bc )
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

function generate_python3_segfault() {
    rlLog "Generate python segfault"
    su -c will_python3_sigsegv $1
}

function generate_python3_exception() {
    rlLog "Generate unhandled python3 exception"
    su -c will_python3_raise $1
}

function generate_crash_unpack() {
    rlLog "Generate unpackaged crash"
    pushd /tmp
    gcc -o crash_unpack -xc - << 'EOF'
#include <stdlib.h>
int main () { abort(); }
EOF

    su -c ./crash_unpack
    rm crash_unpack
    popd # /tmp
}

function load_abrt_conf() {
    ABRT_CONF_DUMP_LOCATION=`augtool get /files/etc/abrt/abrt.conf/DumpLocation 2>/dev/null | grep =`

    if test -z "$ABRT_CONF_DUMP_LOCATION"; then
        ABRT_CONF_DUMP_LOCATION=$( pkg-config abrt --variable=defaultdumplocation )
    else
        ABRT_CONF_DUMP_LOCATION=`echo $ABRT_CONF_DUMP_LOCATION | cut -d'=' -f2 | tr -d ' '`
    fi

    if test -z "$ABRT_CONF_DUMP_LOCATION" && test -d /var/spool/abrt; then
        ABRT_CONF_DUMP_LOCATION="/var/spool/abrt"
    fi

    if test -z "$ABRT_CONF_DUMP_LOCATION"; then
        ABRT_CONF_DUMP_LOCATION="/var/tmp/abrt"
    fi
}

function prepare() {
    load_abrt_conf

    rm -f -- $ABRT_CONF_DUMP_LOCATION/last-ccpp
    rm -f -- $ABRT_CONF_DUMP_LOCATION/last-via-server
    rm -f "/tmp/abrt-done"

    if [ ! -f /etc/libreport/events.d/test_event.conf ]; then
        rlLog "test_event.conf does not exist. Did you run pre.sh?"
    fi
}

function print_syslog {
    COUNT=${1:-10}
    if grep -q ' 6\.' /etc/redhat-release 2>/dev/null; then
        cat /var/log/messages | grep abrt | tail -n $COUNT
    else
        journalctl -b | grep abrt | tail -n $COUNT
    fi
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

function remove_problem_directory() {
    rlRun "abrt remove -f '$crash_PATH'" 0 "Remove problem directory $crash_PATH"
}
