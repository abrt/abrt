#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrtd-concurrent-processing
#   Description: Checks that abrtd can concurrently process several crashes
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2016 Red Hat, Inc. All rights reserved.
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

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="abrtd-concurrent-processing"
PACKAGE="abrt"

ABRT_CONF="/etc/abrt/abrt.conf"
TEST_EVENT_CONF="/etc/libreport/events.d/${TEST}.conf"
ABRT_RSYSLLOG_FILE="/etc/rsyslog.d/${TEST}.conf"
ABRT_LOG_FILE="/var/log/${TEST}.log"

STAGE1_FILE="/var/spool/abrt/${TEST}_stage1"
STAGE2_FILE="/var/spool/abrt/${TEST}_stage2"
READY_STAGE2_FILE="/var/spool/abrt/${TEST}_stage2_ready"

function create_dump_director
{
    rlRun "mkdir -p ${1}" 0

    echo -n "$TEST" > ${1}/type
    echo -n "$TEST" > ${1}/analyzer
    date +%s     > ${1}/time
    date +%s     > ${1}/last_occurrence

    chown -R root:abrt ${1}
    chmod -R 0750 ${1}

    ls -al ${1}
}

rlJournalStart

    rlPhaseStartSetup
        check_prior_crashes

        load_abrt_conf

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlFileBackup $ABRT_CONF

        service abrtd stop

        sed 's/MaxCrashReportsSize\s*=.*/MaxCrashReportsSize = 200/' -i $ABRT_CONF

        cat > $TEST_EVENT_CONF <<EOF
EVENT=post-create type!=${TEST}
    # create big file > 150MiB
    echo "$TEST - \$(basename \$DUMP_DIR) - Creating huge file"
    dd if=/dev/urandom of=huge count=140 bs=1048576
    # let the test script know
    echo "$TEST - \$(basename \$DUMP_DIR) - Notifying the test script"
    touch /tmp/abrt-done
    # wait until all test cases are done
    echo "$TEST - \$(basename \$DUMP_DIR) - Waiting"
    while [ ! -f $STAGE1_FILE ]; do sleep 0.2; done
    rm -f $STAGE1_FILE
    echo "$TEST - \$(basename \$DUMP_DIR) - Done"

EVENT=post-create type=${TEST}
    echo "$TEST 2 - \$(basename \$DUMP_DIR) - Notifying the script"
    touch ${READY_STAGE2_FILE}
    echo "$TEST 2 - \$(basename \$DUMP_DIR) - Waiting"
    while [ ! -f $STAGE2_FILE ]; do sleep 0.2; done
    rm -f $STAGE2_FILE
    echo "$TEST 2 - \$(basename \$DUMP_DIR) - Done"
EOF
        rm -f $STAGE1_FILE
        rm -f $STAGE2_FILE
        rm -f $READY_STAGE2_FILE

        cat > $ABRT_RSYSLLOG_FILE <<EOF
:programname, startswith, "abrt"	$ABRT_LOG_FILE
EOF
        rm -f $ABRT_LOG_FILE

        service rsyslog restart
        service abrtd start
    rlPhaseEnd

    rlPhaseStartTest "Delete just detected problem directory"

        prepare
        will_segfault
        wait_for_hooks

        REMOVED_DD=$ABRT_CONF_DUMP_LOCATION/removed_problem_directory
        create_dump_director ${REMOVED_DD}.new
        dd if=/dev/zero of=${REMOVED_DD}.new/huge count=80 bs=1048576
        rlRun "mv ${REMOVED_DD}.new ${REMOVED_DD}" 0

        c=0
        while [ -d ${REMOVED_DD} ]
        do
            sleep 0.1
            c=$((c+1))
            if [ $c -gt 20 ]; then
                rlFail "abrtd didn't removed ${REMOVED_DD} in 2s"
                break
            fi
        done

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        prepare
        touch $STAGE1_FILE
        wait_for_hooks
        get_crash_path

        rlAssertNotGrep "does not exist" $ABRT_LOG_FILE

        rm -f $STAGE1_FILE
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Delete problem directory from the incoming queue"

        prepare
        will_segfault
        wait_for_hooks
        sleep 1

        prepare

        rlLog "Create the removed directory"
        REMOVED_DD=$ABRT_CONF_DUMP_LOCATION/removed_problem_directory2
        # create huge of half of the size of currently processed directory
        create_dump_director ${REMOVED_DD}.new
        dd if=/dev/zero of=${REMOVED_DD}.new/huge count=50 bs=1048576
        rlRun "mv ${REMOVED_DD}.new ${REMOVED_DD}" 0
        sleep 1
        rlAssertExists ${REMOVED_DD}

        rlLog "Create the trigger directory"
        NOT_REMOVED_DD=$ABRT_CONF_DUMP_LOCATION/not_removed_problem_directory2
        create_dump_director ${NOT_REMOVED_DD}.new
        dd if=/dev/zero of=${NOT_REMOVED_DD}.new/huge count=10 bs=1048576
        echo "`which will_segfault`" > /${NOT_REMOVED_DD}.new/executable
        echo "`which will_segfault`" > /${NOT_REMOVED_DD}.new/cmdline
        rlRun "mv ${NOT_REMOVED_DD}.new ${NOT_REMOVED_DD}" 0

        c=0
        while [ -d ${REMOVED_DD} ]
        do
            sleep 0.1
            c=$((c+1))
            if [ $c -gt 20 ]; then
                rlFail "abrtd didn't removed ${REMOVED_DD} in 2s"
                break
            fi
        done

        rlAssertExists ${NOT_REMOVED_DD}

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        touch $STAGE1_FILE

        wait_for_hooks
        get_crash_path
        rlRun "abrt-cli rm $crash_PATH"
        sleep 1

        while [ ! -f ${READY_STAGE2_FILE} ]
        do
            sleep 0.1
            c=$((c+1))
            if [ $c -gt 2000 ]; then
                rlFail "stage2 didn't started in 200s"
                break
            fi
        done
        rm -f ${READY_STAGE2_FILE}

        prepare
        touch $STAGE2_FILE
        wait_for_hooks
        get_crash_path

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        rlAssertNotGrep "does not exist" $ABRT_LOG_FILE

        rm -f $STAGE1_FILE
        rm -f $STAGE2_FILE
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Delete problem directory that was already processed"

        prepare
        will_segfault
        wait_for_hooks

        prepare
        touch $STAGE1_FILE
        wait_for_hooks
        get_crash_path
        sleep 1

        prepare

        rlLog "Create the break directory"
        BREAK_DD=$ABRT_CONF_DUMP_LOCATION/break_directory
        # create huge of half of the size of currently processed directory
        create_dump_director ${BREAK_DD}.new
        dd if=/dev/zero of=${BREAK_DD}.new/huge count=50 bs=1048576
        echo "`which will_abort`" > /${BREAK_DD}.new/executable
        echo "`which will_abort`" > /${BREAK_DD}.new/cmdline
        rlRun "mv ${BREAK_DD}.new ${BREAK_DD}" 0
        sleep 1
        rlAssertExists ${BREAK_DD}

        while [ ! -f ${READY_STAGE2_FILE} ]
        do
            sleep 0.1
            c=$((c+1))
            if [ $c -gt 2000 ]; then
                rlFail "stage2 didn't started in 200s"
                break
            fi
        done
        rm -f ${READY_STAGE2_FILE}

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        rlLog "Create the trigger directory"
        NOT_REMOVED_DD=$ABRT_CONF_DUMP_LOCATION/not_removed_problem_directory3
        create_dump_director ${NOT_REMOVED_DD}.new
        dd if=/dev/zero of=${NOT_REMOVED_DD}.new/huge count=10 bs=1048576
        echo "`which will_segfault`" > /${NOT_REMOVED_DD}.new/executable
        echo "`which will_segfault`" > /${NOT_REMOVED_DD}.new/cmdline
        rlRun "mv ${NOT_REMOVED_DD}.new ${NOT_REMOVED_DD}" 0

        c=0
        while [ -d ${crash_PATH} ]
        do
            sleep 0.1
            c=$((c+1))
            if [ $c -gt 20 ]; then
                rlFail "abrtd didn't removed $crash_PATH in 2s"
                break
            fi
        done

        rlAssertExists ${BREAK_DD}
        rlAssertExists ${NOT_REMOVED_DD}

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        prepare
        touch $STAGE2_FILE
        wait_for_hooks
        get_crash_path
        rlRun "abrt-cli rm $crash_PATH"
        sleep 1

        prepare
        touch $STAGE2_FILE
        wait_for_hooks
        get_crash_path
        rlRun "abrt-cli rm $crash_PATH"

        rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

        rlAssertNotGrep "does not exist" $ABRT_LOG_FILE

        rm -f $STAGE1_FILE
        rm -f $STAGE2_FILE
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore

        rlBundleLogs abr $ABRT_LOG_FILE

        rm -f $TEST_EVENT_CONF
        rm -f $ABRT_RSYSLLOG_FILE
        rm -f /tmp/abrt_done*
        rm -rf /var/spool/abrt/*
        rm -f $ABRT_LOG_FILE

        service abrtd restart
        service rsyslog restart
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
