#!/bin/bash
export ABRT_TESTOUT_ROOT="/tmp/abrt-TESTOUT"
export MAILTO="abrt-devel-list@redhat.com"
export MAILFROM="abrt-testsuite-bot@redhat.com"

# list of tests which will cause testing to stop if failed
# may be supressed with TEST_CONTINUE
export TEST_CRITICAL='abrt-nightly-build'

# continue testing even if critical test fails
export TEST_CONTINUE=1

# shutdown vm after testing
export SHUTDOWN=0

function echo_err {
    echo "ERROR: $1"
    exit 1
}
