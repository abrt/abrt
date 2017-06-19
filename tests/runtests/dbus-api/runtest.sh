#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-api
#   Description: Check dbus-api functionality
#   Author: Richard Marko <rmarko@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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

TEST="dbus-api"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetProblems &> dbus_reply.log"

        rlAssertGrep "array" dbus_reply.log
        rlAssertGrep "string" dbus_reply.log
        rlAssertGrep "$crash_PATH" dbus_reply.log

        # we need to remove core_backtrace so it doesn't
        # mark next crash as a duplicate
        rm -f "$crash_PATH/core_backtrace"

        prepare
        generate_second_crash
        wait_for_hooks

        rlAssertGreater "Second crash recorded" $(abrt-cli list | wc -l) 0
        crash2_PATH="$(abrt-cli list | grep Directory \
            | grep -v "$crash_PATH" \
            | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash2_PATH" ]; then
            rlDie "No crash dir generated for second crash, this shouldn't happen"
        fi

        rlRun "dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetProblems &> dbus_second_reply.log"

        rlAssertGrep "array" dbus_second_reply.log
        rlAssertGrep "$crash_PATH" dbus_second_reply.log
        rlAssertGrep "$crash2_PATH" dbus_second_reply.log

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlRun "abrt-cli rm $crash2_PATH" 0 "Remove second crash directory"
    rlPhaseEnd

    rlPhaseStartTest "FindProblemByElementInTimeRange"

        time_from=`date +%s`
        generate_crash
        wait_for_hooks
        get_crash_path

        cmd_line=`cat $crash_PATH/cmdline`

        rlRun "dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.FindProblemByElementInTimeRange string:cmdline string:${cmd_line} int64:${time_from} int64:`date +%s` boolean:true &> dbus_reply.log"

        rlAssertGrep "array" dbus_reply.log
        rlAssertGrep "$crash_PATH" dbus_reply.log

    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlBundleLogs abrt *.log
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
