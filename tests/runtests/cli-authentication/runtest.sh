#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of cli-authentication
#   Description: tests abrt-cli's ability to work with the system problems
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

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="cli-authentication"
PACKAGE="abrt"
RUNTESTS_EVENT_CONF_FILE="/etc/libreport/events.d/runtests_events.conf"

function normalize_file
{
    rlRun "tail -$(wc -l $2 | cut -f1 -d' ') $1 | sed 's/\o033\[0m//' | tr -d '\r' | $3 | tee $1.norm"
}

rlJournalStart
    rlPhaseStartSetup
        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)

        cp expect /tmp/

        pushd $TmpDir

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "useradd abrt-unprivileged"
        rlRun "echo ribbit | passwd abrt-unprivileged --stdin"
        rlRun "gpasswd -a abrt-unprivileged wheel"

        rlLog "Creating $RUNTESTS_EVENT_CONF_FILE"
        cat > $RUNTESTS_EVENT_CONF_FILE <<EOF
EVENT=report-cli type=runtests analyzer=runtests
    echo "It works!" > have_been_here
EOF
    rlPhaseEnd

    rlPhaseStartTest "list"
        rlRun "abrt-cli list 2>&1 | sort | tee owner-list.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a list 2>&1 | tee auth-list.log"
        normalize_file auth-list.log owner-list.log sort
        rlAssertNotDiffer owner-list.log auth-list.log.norm
    rlPhaseEnd

    rlPhaseStartTest "info"
        rlRun "abrt-cli info $crash_PATH 2>&1 | tee owner-info.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a info $crash_PATH 2>&1 | tee auth-info.log"
        normalize_file auth-info.log owner-info.log cat
        rlAssertNotDiffer owner-info.log auth-info.log.norm
    rlPhaseEnd

# TODO: Investigate further why this test sometimes fails.
#    rlPhaseStartTest "process info"
#        # Remove several big text files to avoid reaching out the size limits"
#        rlRun "rm -f $crash_PATH/mountinfo"
#        rlRun "rm -f $crash_PATH/environ"
#        rlRun "rm -f $crash_PATH/maps"
#
#        rlRun "abrt-cli info -d $crash_PATH 2>&1 | tee owner-detailed-info.log"
#        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a process info 2>&1 | tee process-auth-info.log"
#        normalize_file process-auth-info.log owner-detailed-info.log cat
#        rlAssertNotDiffer owner-detailed-info.log process-auth-info.log.norm
#    rlPhaseEnd

    rlPhaseStartTest "status"
        rlRun "abrt-cli status 2>&1 | tee owner-status.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a status 2>&1 | tee auth-status.log"
        normalize_file auth-status.log owner-status.log cat
        rlAssertNotDiffer owner-status.log auth-status.log.norm
    rlPhaseEnd

    rlPhaseStartTest "remove"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a remove $crash_PATH 2>&1 | tee auth-remove.log"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartTest "process remove"
        prepare
        generate_python_segfault
        wait_for_hooks
        get_crash_path

        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a process remove 2>&1 | tee process-auth-remove.log"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartTest "report"
        prepare
        generate_second_crash
        wait_for_hooks
        get_crash_path

        rlLog "Making the crash reportable with $RUNTESTS_EVENT_CONF_FILE"
        rlRun "echo -n runtests > $crash_PATH/type"
        rlRun "echo -n runtests > $crash_PATH/analyzer"

        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a report $crash_PATH 2>&1 | tee auth-report.log"
        rlAssertExists $crash_PATH/have_been_here
        rlAssertGrep "It works!" $crash_PATH/have_been_here

        rlRun "abrt-cli rm $crash_PATH"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartTest "process report"
        sleep 1000 &
        sleep 1
        kill -ABRT %%
        wait_for_hooks
        get_crash_path

        rlLog "Making the crash reportable with $RUNTESTS_EVENT_CONF_FILE"
        rlRun "echo -n runtests > $crash_PATH/type"
        rlRun "echo -n runtests > $crash_PATH/analyzer"

        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a process report 2>&1 | tee auth-report.log"
        rlAssertExists $crash_PATH/have_been_here
        rlAssertGrep "It works!" $crash_PATH/have_been_here

        rlRun "abrt-cli rm $crash_PATH"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt-cli-authetication $(ls *.log *.norm)
        rlRun "rm $RUNTESTS_EVENT_CONF_FILE"
        userdel -f -r abrt-unprivileged
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd

