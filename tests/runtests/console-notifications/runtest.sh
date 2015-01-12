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

rlJournalStart
    rlPhaseStartSetup
        rlAssertExists /etc/profile.d/abrt-console-notification.sh || rlDie "Nothing to test - install abrt-console-notifications first"

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

        rlLog "ABRT should suggest user to run abrt-cli since"
        CANARY="CANARY_$(date +%s)_LIVES"
        # bash allows to run interactive shell with -c
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertExists $LNPATH

        # ABRT worked as expected
        rlAssertGrep "ABRT has detected .* problem(s). For more info run: abrt-cli list" $LOG_NAME
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

        rlLog "ABRT should suggest user to run abrt-cli list with --since"
        rlRun "TS=$(cat $LNPATH)"
        CANARY="CANARY_$(date +%s)_LIVES"
        # bash allows to run interactive shell with -c
        sh -l -i -c "echo $CANARY" >$LOG_NAME 2>&1

        rlAssertExists $LNPATH

        # ABRT updated the file
        rlAssertNotEquals "The time stamp file has been updated" "_$TN" "_$(cat $LNPATH)"

        # ABRT worked as expected
        rlAssertGrep "ABRT has detected .* problem(s). For more info run: abrt-cli list --since $TS" $LOG_NAME
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
        su adm -s /usr/bin/sh -c "sh -l -i -c \"echo $CANARY\"" >$LOG_NAME 2>&1

        rlAssertNotExists $LNPATH

        # ABRT silently failed to create home
        rlAssertNotGrep "cannot create directory" $LOG_NAME
        # ABRT didn't run
        rlAssertNotGrep "ABRT has" $LOG_NAME
        # ABRT didn't break login
        rlAssertGrep "$CANARY" $LOG_NAME
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs "console_notifications" *.log
        popd # TmpDir
        rm -rf $TmpDir
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
