#!/bin/bash

# - global config -
# logs destination
export OUTPUT_ROOT='/tmp/abrt-testsuite'

export TEST_LIST='aux/test_order'

export PRE_SCRIPT='aux/pre.sh'
export TEST_SCRIPT='aux/run_in_order.sh'
export RUNNER_SCRIPT='aux/runner.sh'
export FORMAT_SCRIPT='aux/no_format.sh'
export REPORT_SCRIPT='aux/no_report.sh'
export POST_SCRIPT='aux/post.sh'

# - run script config -
# list of tests which will cause testing to stop if failed
# may be supressed with TEST_CONTINUE
export TEST_CRITICAL='abrt-nightly-build'
# continue testing even if critical test fails
export TEST_CONTINUE=0
# wait $DELAY seconds before running next script
export DELAY=30

# - mailx script config -
export MAILTO='rmarko@redhat.com'
export MAILFROM='abrt-testsuite-bot@redhat.com'

# - scp script config -
export SCPTO='exampleuser@example.org:/var/abrt/results/'

# - post script config -
# shutdown machine after testing
export SHUTDOWN=0
