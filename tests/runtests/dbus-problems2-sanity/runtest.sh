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

        TmpDir=$(mktemp -d)
        cp -r ./cases $TmpDir
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
    rlPhaseEnd

    rlPhaseStartTest
        for test_fixture in `ls cases/test_*.py`
        do
            # [  LOG   ] ++++ Starting: cases/test_foo.py ++++
            # [  PASS  ] Command 'python3 cases/test_foo.py 12345' ..
            # [  FAIL  ] Failed to kill 54321
            # [  FAIL  ] The dump location is tainted
            # [  LOG   ] ++++ Finished: cases/test_foo.py ++++
            #
            rlLog "++++ Starting: $test_fixture ++++"

            which abrt-dbus
            abrt-dbus -vvv -t 100 &> abrt_dbus_${test_fixture#cases/}.log &
            ABRT_DBUS_PID=$!
            ps aux | grep abrt-dbus

            rlRun "python3 $test_fixture $TEST_USER_UID"

            sleep 2
            kill $ABRT_DBUS_PID || {
                rlLog "Failed to kill $ABRT_DBUS_PID"
                kill -9 $ABRT_DBUS_PID
            }

            DIRECTORIES=`find $ABRT_CONF_DUMP_LOCATION -mindepth 1 -maxdepth 1 -type d`
            if [ -n "$DIRECTORIES" ]; then
                rlFail "The dump location is tainted"
                rlLog `ls -al $ABRT_CONF_DUMP_LOCATION`
                rm -rf $ABRT_CONF_DUMP_LOCATION/*
            fi

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
