#!/bin/bash

function check_prior_crashes() {
    rlAssert0 "No prior crashes recorded" $(abrt-cli list | wc -l)
    if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
        rlDie "Won't proceed"
    fi
}

function get_crash_path() {
    rlLog "Get crash path"
    rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
    crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
    if [ ! -d "$crash_PATH" ]; then
        rlDie "No crash dir generated, this shouldn't happen"
    fi
    rlLog "PATH = $crash_PATH"
}

function generate_crash() {
    rlLog "Generate crash"
    will_segfault
}

function generate_second_crash() {
    rlLog "Generate second crash"
    will_abort
}
