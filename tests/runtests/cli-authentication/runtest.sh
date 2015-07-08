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

rlJournalStart
    rlPhaseStartSetup
        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)

        cp expect /tmp/

        pushd $TmpDir

        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "useradd abrt-unprivileged"
        rlRun "echo ribbit | passwd abrt-unprivileged --stdin"
        rlRun "gpasswd -a abrt-unprivileged wheel"
    rlPhaseEnd

    rlPhaseStartTest "list"
        rlRun "abrt-cli -a list 2>&1 | tee owner-list.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a list 2>&1 | tee auth-list.log"
        rlRun "tail -$(wc -l owner-list.log | cut -f1 -d' ') auth-list.log | sed 's/\o033\[0m//' | tr -d '\r' | tee auth-list.tmp"
        rlAssertNotDiffer owner-list.log auth-list.tmp
    rlPhaseEnd

    rlPhaseStartTest "info"
        rlRun "abrt-cli info $crash_PATH 2>&1 | tee owner-info.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a info $crash_PATH 2>&1 | tee auth-info.log"
        rlRun "tail -$(wc -l owner-info.log | cut -f1 -d' ') auth-info.log | sed 's/\o033\[0m//' | tr -d '\r' | tee auth-info.tmp"
        rlAssertNotDiffer owner-info.log auth-info.tmp
    rlPhaseEnd

    rlPhaseStartTest "status"
        rlRun "abrt-cli status 2>&1 | tee owner-status.log"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a status 2>&1 | tee auth-status.log"
        rlRun "tail -$(wc -l owner-status.log | cut -f1 -d' ') auth-status.log | sed 's/\o033\[0m//' | tr -d '\r' | tee auth-status.tmp"
        rlAssertNotDiffer owner-status.log auth-status.tmp
    rlPhaseEnd

    rlPhaseStartTest "remove"
        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a remove $crash_PATH 2>&1 | tee auth-remove.log"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartTest "report"
        # abrt-ccpp could ignore this crash due to short period of time between
        # two crashes of the same executable
        sleep 30

        generate_crash
        wait_for_hooks
        get_crash_path

        rlLog "Creating $RUNTESTS_EVENT_CONF_FILE"
        cat > $RUNTESTS_EVENT_CONF_FILE <<EOF
EVENT=report-cli type=runtests analyzer=runtests
    echo "It works!" > have_been_here
EOF

        rlLog "Making the crash reportable with $RUNTESTS_EVENT_CONF_FILE"
        rlRun "echo -n runtests > $crash_PATH/type"
        rlRun "echo -n runtests > $crash_PATH/analyzer"

        rlRun "sudo -u abrt-unprivileged /tmp/expect abrt-cli -a report $crash_PATH 2>&1 | tee auth-report.log"
        rlAssertExists $crash_PATH/have_been_here
        rlAssertGrep "It works!" $crash_PATH/have_been_here

        rlRun "rm $RUNTESTS_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt-cli-authetication $(ls *.log)
        rlRun "abrt-cli rm $crash_PATH"
        userdel -f -r abrt-unprivileged
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd

