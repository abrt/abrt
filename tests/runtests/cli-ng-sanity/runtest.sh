#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of cli-ng-sanity
#   Description: does sanity on abrt-cli-ng
#   Author: Richard Marko <rmarko@redhat.com>
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

TEST="cli-ng-sanity"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup

        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "--version"
        rlRun "abrt -v 2>&1 | grep 'abrt'"
        rlRun "abrt --version 2>&1 | grep 'abrt'"
    rlPhaseEnd

    rlPhaseStartTest "--help"
        rlRun "abrt --help" 0
        rlRun "abrt --help 2>&1 | grep 'usage: abrt'"
    rlPhaseEnd

    rlPhaseStartTest "list"
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "abrt list | grep -i 'Id'"
        rlRun "abrt list | grep -i 'Component'"
        rlRun "abrt list --pretty full | grep -i 'Command line'"
    rlPhaseEnd

    rlPhaseStartTest "list -n" # list not-reported
        rlRun "abrt list -n | grep -i 'Id'"
        rlRun "abrt list -n | grep -i 'Component'"
    rlPhaseEnd

    rlPhaseStartTest "status"
        rlRun "abrt status | grep 'has detected a problem'"
    rlPhaseEnd

    rlPhaseStartTest "report NONEXISTENT"
        rlRun "abrt report NONEXISTENT" 1
    rlPhaseEnd

    rlPhaseStartTest "report not-reportable"
        rlRun "touch $crash_PATH/not-reportable"

        cp $crash_PATH/{type,analyzer} ./

        echo "cli_sanity_test_not_reportable" > $crash_PATH/type
        echo "cli_sanity_test_not_reportable" > $crash_PATH/analyzer

        PROBLEM_ID=$(abrt list --format={short_id})
        rlRun "abrt report 2>&1 | tee abrt-cli-report-not-reportable.log" 0
        rlAssertGrep "Problem '$PROBLEM_ID' cannot be reported" abrt-cli-report-not-reportable.log

        cp -f type analyzer $crash_PATH

        rlRun "rm -f $crash_PATH/not-reportable"
    rlPhaseEnd

    rlPhaseStartTest "info DIR"
        rlRun "abrt info"
        rlRun "abrt info --pretty email > info.out"
    rlPhaseEnd

    rlPhaseStartTest "list (after reporting)"
        DIR=$( abrt list --pretty full | grep 'Path' | head -n1 | awk '{ print $2 }' )

        # this should ensure that ABRT will consider the problem as reported
        rlRun "reporter-print -r -d $DIR -o /dev/null"

        # this expects that reporter-print works and adds an URL to
        # the output file to the problem's data
        rlRun "abrt list | grep -i 'file:///dev/null'"
    rlPhaseEnd

    rlPhaseStartTest "list -n (after reporting)" # list not-reported
        rlRun "abrt list -n | grep 'No problems'"
    rlPhaseEnd

    rlPhaseStartTest "info NONEXISTENT"
        rlRun "abrt info NONEXISTENT" 1
    rlPhaseEnd

    rlPhaseStartTest "remove NONEXISTENT"
        rlRun "abrt remove NONEXISTENT" 1
    rlPhaseEnd

    rlPhaseStartTest "remove DIR"
        rlRun "abrt remove -f"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
