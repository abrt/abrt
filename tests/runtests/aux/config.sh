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
#export DELAY=30

# - pre script config -
export REINSTALL_PRE=1
export UPDATE_SYSTEM=0
export UPDATE_PACKAGES=0
export DISABLE_NOAUDIT=0
export DISABLE_GPGCHECK=0
export DISABLE_AUTOREPORTING=1
export STORE_CONFIGS=1

# - mailx script config -
export MAILTO='rmarko@redhat.com'
export MAILFROM='abrt-testsuite-bot@redhat.com'

# - scp script config -
export SCPTO='exampleuser@example.org:/var/abrt/results/'
export SCPOPTS="-o StrictHostKeyChecking=no"

# - post script config -
# shutdown machine after testing
export SHUTDOWN=0

# - runner script config -
export REINSTALL_BEFORE_EACH_TEST=0
export RESTORE_CONFIGS_BEFORE_EACH_TEST=1
export CLEAN_SPOOL_BEFORE_EACH_TEST=1
export DUMP_PACKAGE_VERSIONS=1
# Ensures that a test will not hang forever.
# See man timeout for more details about the format.
export TEST_TIMEOUT=15m

# - misc
export PACKAGES="abrt \
                 abrt-desktop \
                 abrt-cli \
                 abrt-devel \
                 abrt-python \
                 abrt-console-notification \
                 libreport \
                 libreport-plugin-bugzilla \
                 libreport-plugin-rhtsupport \
                 libreport-plugin-reportuploader \
                 libreport-plugin-mailx \
                 libreport-plugin-ureport \
                 libreport-plugin-logger \
                 libreport-plugin-mantisbt \
                 satyr \
                 will-crash"
