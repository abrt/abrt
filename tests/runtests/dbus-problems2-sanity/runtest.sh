#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-problems2-sanity
#   Description: Check Problems2 D-Bus API
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
#
#   This program is free software: you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of
#   the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE.  See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program. If not, see http://www.gnu.org/licenses/.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   If you want to test development version of abrt-dbus run
#   the following command:
#
#     $ sudo PATH=$ADD_YOUR_PATH/abrt/src/dbus:$PATH ./runtest.sh
#
#   If you want to run a single test fixture:
#
#     $ cd cases && sudo ./test_whatever $WHEEL_MEMBER_UID
#
#   or if you need to run a single test:
#
#     $ cd cases && sudo ./test_whatever $WHEEL_MEMBER_UID $TEST_NAME
#
#   Every test fixture supports "--log=[...|INFO|DEBUG|...]" command line
#   argument. https://docs.python.org/3/library/logging.html#logging-levels
#
#   The runtest.sh can print more verbose message if you set ABRT_VERBOSE=true.
#
#   You can select executed test by setting ABRT_TEST_LIST env to space
#   separated list of file paths.
#
#   You can pass extra command line arguments to the executed test through
#   ABRT_TEST_ARGS env.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="dbus-problems2-sanity"
PACKAGE="abrt-dbus"
TEST_USER="abrt-dbus-test"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        load_abrt_conf

        if [ -z "$ABRT_VERBOSE" ]; then
            ABRT_VERBOSE=false
        fi

        TmpDir=$(mktemp -d)
        cp -r ./cases $TmpDir
        cp valgrind-suppressions $TmpDir
        pushd $TmpDir

        useradd $TEST_USER -M -g wheel || rlDie "Cannot proceed without the user"
        rlLog "Added user: $TEST_USER"

        echo "kokotice" | passwd $TEST_USER --stdin || {
            userdel -r -f $TEST_USER
            rlLog "Removed user: $TEST_USER"
            rlDie "Failed to update password"
        }

        TEST_USER_UID=$(id -u $TEST_USER | tr -d "\n")

        killall abrt-dbus

        export ABRT_DBUS_DATA_SIZE_LIMIT=$((256*1024*1024))

        if [ -z "$ABRT_TEST_LIST" ]; then
            ABRT_TEST_LIST="$(ls -1 cases/test_*.py)"
        fi
    rlPhaseEnd

    rlPhaseStartTest
        for test_fixture in $ABRT_TEST_LIST
        do
            # [  LOG   ] ++++ Starting: cases/test_foo.py ++++
            # [  PASS  ] Command 'python3 cases/test_foo.py 12345' ..
            # [  FAIL  ] Failed to kill 54321
            # [  FAIL  ] The dump location is tainted
            # [  LOG   ] ++++ Finished: cases/test_foo.py ++++
            #
            rlLog "++++ Starting: $test_fixture ++++"

            ABRT_DBUS=`which abrt-dbus`
            ABRT_DBUS_MIME=`file -b --mime-type $ABRT_DBUS`

            CMD_PREFIX=""
            if [ "$ABRT_DBUS_MIME" == "text/x-shellscript" ]; then
                CMD_PREFIX="libtool --mode=execute"
            fi

            ABRT_DBUS_LOG_FILE=abrt_dbus_${test_fixture#cases/}.log
            touch $ABRT_DBUS_LOG_FILE
            echo "ABRT_DBUS_LOG_FILE=$(realpath $ABRT_DBUS_LOG_FILE)"

            VALGRIND_LOG_FILE=valgrind_${test_fixture#cases/}.log
            touch $VALGRIND_LOG_FILE
            echo "VALGRIND_LOG_FILE=$(realpath $VALGRIND_LOG_FILE)"

            $CMD_PREFIX \
            valgrind --leak-check=full \
                     --num-callers=16 \
                     --show-leak-kinds=definite \
                     --trace-children=no \
                     --quiet --gen-suppressions=all \
                     --suppressions=valgrind-suppressions \
                     --log-file=$VALGRIND_LOG_FILE \
            $ABRT_DBUS -vvv -t 100 &> $ABRT_DBUS_LOG_FILE &
            ABRT_DBUS_PID=$!

            # Giving abrt-dbus some time to accommodate under valgrind
            sleep 5
            ABRT_DBUS_FD_BEFORE=abrt_dbus_fd_${ABRT_DBUS_PID}_before.log
            ABRT_DBUS_FD_AFTER=abrt_dbus_fd_${ABRT_DBUS_PID}_after.log

            ls -l /proc/$ABRT_DBUS_PID/fd > $ABRT_DBUS_FD_BEFORE

            # Let people know what was actually executed.
            ps aux | grep abrt-dbus | grep -v grep

            # Run the test!!!
            rlRun "python3 $test_fixture $TEST_USER_UID $ABRT_TEST_ARGS"

            # Giving abrt-dbus some time to finish its tasks under valgrind
            sleep 5
            ls -l /proc/$ABRT_DBUS_PID/fd > $ABRT_DBUS_FD_AFTER

            kill $ABRT_DBUS_PID || {
                rlLog "Failed to kill $ABRT_DBUS_PID"
                kill -9 $ABRT_DBUS_PID
            }

            # Giving valgrind some time to print out its logs
            sleep 2

            DIRECTORIES=`find $ABRT_CONF_DUMP_LOCATION -mindepth 1 -maxdepth 1 -type d`
            if [ -n "$DIRECTORIES" ]; then
                rlFail "Not all dump directories were removed"
                if $ABRT_VERBOSE; then
                    rlLog "++"
                    rlLog "++ BEGIN: $ABRT_CONF_DUMP_LOCATION"
                    rlLog "`ls -l $ABRT_CONF_DUMP_LOCATION`"
                    rlLog "++ END: $ABRT_CONF_DUMP_LOCATION"
                    rlLog "++"
                fi
                rm -rf $ABRT_CONF_DUMP_LOCATION/*
            else
                rlPass "No remaining dump directories"
            fi

            FDS=`diff -u ${ABRT_DBUS_FD_BEFORE} ${ABRT_DBUS_FD_AFTER}`
            if [ -n "$FDS" ]; then
                rlFail "Some new FDs are still opened"
                if $ABRT_VERBOSE; then
                    rlLog "++"
                    rlLog "++ BEGIN: /proc/[pid]/fd"
                    rlLog "$FDS"
                    rlLog "++ END: /proc/[pid]/fd"
                    rlLog "++"
                fi
            else
                rlPass "No file descriptor leaks"
                # Do not polute log archive with useless files
                rm $ABRT_DBUS_FD_BEFORE
                rm $ABRT_DBUS_FD_AFTER
            fi

            VGLINES=`cat $VALGRIND_LOG_FILE | wc -l`
            if [ 0 -ne $VGLINES ]; then
                rlFail "Some memory error or leak encountered"
                if $ABRT_VERBOSE; then
                    rlLog "++"
                    rlLog "++ BEGIN: valgrind"
                    rlLog "`cat $VALGRIND_LOG_FILE`"
                    rlLog "++ END: valgrind"
                    rlLog "++"
                fi
            else
                rlPass "No memory leaks and errors"
                # Do not polute log archive with empty files
                rm $VALGRIND_LOG_FILE
            fi

            rm -f /tmp/shorttext
            rm -f /tmp/hugetext
            rm -f /tmp/fake_type
            rlLog "++++ Finished: $test_fixture ++++"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls *.log)

        userdel -r -f $TEST_USER
        rlLog "Removed user: $TEST_USER"

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
