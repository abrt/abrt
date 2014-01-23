#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of cli-list-rhtsupport
#   Description: checks whether 'abrt-cli list' output states that reports will go to Red Hat Customer Portal
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

TEST="cli-list-rhtsupport"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        pushd $TmpDir

        OLD_AUTO_REPORTING_CONFIG=$(abrt-auto-reporting)
        rlRun "abrt-auto-reporting disabled"
    rlPhaseEnd

    rlPhaseStartTest "Not reported problem"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "rm -f $crash_PATH/reported_to $crash_PATH/not-reportable"

        rlRun "abrt-cli list >out.log"
        rlAssertGrep "Run 'abrt-cli report .*' for creating a case in Red Hat Customer Portal" out.log

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Already reported problem"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "rm -f $crash_PATH/not-reportable"

        rlRun "echo 'RHTSupport: TIME=123456789 URL=https://portal.redhat.com/7777' > $crash_PATH/reported_to"

        rlRun "abrt-cli list >out-reported.log"
        rlAssertNotGrep "Run 'abrt-cli report.*' for creating a case in Red Hat Customer Portal" out-reported.log

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Not-reportable problem"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "rm -f $crash_PATH/reported_to $crash_PATH/not-reportable"

        rlRun "echo 'This problem is not reportable because I wish it' > $crash_PATH/not-reportable"

        rlRun "abrt-cli list >out-notreportable.log"
        rlAssertNotGrep "Run 'abrt-cli report.*' for creating a case in Red Hat Customer Portal" out-notreportable.log

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Not reported problem and unavailable RHTSupport plugin"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "echo 'TestFakeType' > $crash_PATH/type"
        rlRun "echo 'TestFakeType' > $crash_PATH/analyzer"

        rlRun "rm -f $crash_PATH/reported_to $crash_PATH/not-reportable"

        rlRun "abrt-cli list >out-no-plugin.log"
        rlAssertNotGrep "Run 'abrt-cli report .*' for creating a case in Red Hat Customer Portal" out-no-plugin.log

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-auto-reporting $OLD_AUTO_REPORTING_CONFIG"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd

