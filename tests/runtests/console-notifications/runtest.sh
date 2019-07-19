#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of console-notifications
#   Description: Sanity test for console notifications
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2014 Red Hat, Inc. All rights reserved.
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

TEST="console-notifications"
PACKAGE="abrt"
RELPATH=".cache/abrt/lastnotification"
LNPATH="$HOME/$RELPATH"
ABRT_CONF_DUMP_LOCATION="/var/spool/abrt"

function abrtCreateNewProblem() {
    DUMPDIR_PATH=$ABRT_CONF_DUMP_LOCATION/ccpp-$(date +%F-%T-%N)
    mkdir $DUMPDIR_PATH

    echo -n "CCpp" > $DUMPDIR_PATH/type
    echo -n "abrt-ccpp" > $DUMPDIR_PATH/analyzer
    echo -n "/usr/bin/will_segfault" > $DUMPDIR_PATH/executable
    echo -n $(date +%s.%N) > $DUMPDIR_PATH/uuid
    echo -n "will-crash" > $DUMPDIR_PATH/component
    echo -n $(date +%s) > $DUMPDIR_PATH/time
}

rlJournalStart
    rlPhaseStartSetup
        rlAssertExists /etc/profile.d/abrt-console-notification.sh || rlDie "Nothing to test - install abrt-console-notification first"

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlRun "rm --preserve-root -fv $LNPATH"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path
    rlPhaseEnd

    rlPhaseStartTest "interactive shell first run"
        LOG_NAME="interactive_shell_first_run.log"
        rlAssertNotExists $LNPATH

        rlLog "ABRT should suggest user to run abrt since"
        CANARY="CANARY_$(date +%s)_LIVES"
        # bash allows to run interactive shell with -c
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertExists $LNPATH

        # ABRT worked as expected
        rlAssertGrep "ABRT has detected .* problem(s).*For more info run:.*abrt list.*" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "interactive shell second run"
        LOG_NAME="interactive_shell_second_run.log"

        # make sure we don't run in same second
        rlLog "sleep 2"
        sleep 2

        rlLog "ABRT should not output anything unless an unexpected crash happened"
        CANARY="CANARY_$(date +%s)_LIVES"
        # bash allows to run interactive shell with -c
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertExists $LNPATH

        # ABRT didn't output anything
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "interactive shell new occurrence"
        LOG_NAME="interactive_shell_new_occurrence.log"

        # ABRT ignores crashes which occur within 30s
        rlLog "sleep 30"
        sleep 30
        prepare
        generate_crash
        wait_for_hooks

        rlLog "ABRT should suggest user to run abrt list with --since"
        rlRun "TS=$(cat $LNPATH)"
        CANARY="CANARY_$(date +%s)_LIVES"
        # bash allows to run interactive shell with -c
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertExists $LNPATH

        # ABRT updated the file
        rlAssertNotEquals "The time stamp file has been updated" "_$TN" "_$(cat $LNPATH)"

        # ABRT worked as expected
        rlAssertGrep "ABRT has detected .* problem(s).*For more info run:.*abrt list --since $TS.*" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "interactive shell without tty"
        LOG_NAME="interactive_shell_without_tty.log"
        rlRun "rm --preserve-root -fv $LNPATH"

        rlLog "Start interactive shell without TTY"
        CANARY="CANARY_$(date +%s)_LIVES"
        # forwarding STDIN closes TTY
        sh -l -i <<< "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertNotExists $LNPATH

        # ABRT didn't run
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "non-interactive shell"
        LOG_NAME="noninteractive_shell.log"
        rlRun "rm --preserve-root -fv $LNPATH"

        rlLog "Run a shell script with --login"
        CANARY="CANARY_$(date +%s)_LIVES"
        sh -l -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertNotExists $LNPATH

        # ABRT didn't run in non-interactive shell
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "interactive shell empty home"
        LOG_NAME="$(PWD)/interactive_shell_empty_home.log"
        rlRun "rm --preserve-root -fv $LNPATH"

        rlLog "Clear HOME env variable and run interactive shell"
        CANARY="CANARY_$(date +%s)_LIVES"
        (export HOME="" sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1)

        rlAssertNotExists $LNPATH
        rlAssertNotExists "/$RELPATH"

        # ABRT didn't run
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "interactive shell not writable home"
        LOG_NAME="interactive_shell_no_writable_home.log"
        rlRun "rm --preserve-root -fv $LNPATH"

        rlLog "Login as 'adm' user whose home is /var/adm and is not writable for him"
        CANARY="CANARY_$(date +%s)_LIVES"
        su adm -s /bin/sh -c "sh -l -i -c \"echo $CANARY\"" >$LOG_NAME 2>&1

        rlAssertNotExists $LNPATH

        # ABRT silently failed to create home
        rlAssertNotGrep "cannot create directory" $LOG_NAME
        # ABRT didn't run
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartTest "time out"
        rlLog "This part of the test can sometimes fail. If this happens, run the test again"
        LOG_NAME="timeout.log"

        # creating massive amounts of dump dirs to ensure 'abrt status'
        # processing takes longer than console-notification's time out allows
        num_of_dump_dirs=10000
        rlLog "Creating $num_of_dump_dirs dumpdirs"
        for i in $(seq 1 $num_of_dump_dirs); do
            abrtCreateNewProblem
            if [ $(($i % $(($num_of_dump_dirs/10)))) -eq 0 ]; then
                rlLog "Created $i dump dirs from $num_of_dump_dirs"
            fi
        done

        (sleep 15; killall abrt > killall.log 2>&1) &

        rlLog "ABRT emits a warning due to time out of abrt status"
        CANARY="CANARY_$(date +%s)_LIVES"
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        # ABRT worked as expected
        rlAssertGrep "'abrt status' timed out|Can't get problem list from abrt-dbus: Timeout was reached
        " $LOG_NAME -E
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME

        sleep 6
        rlAssertGrep "abrt: no process .*" killall.log

        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/*"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs "console_notifications" *.log
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
