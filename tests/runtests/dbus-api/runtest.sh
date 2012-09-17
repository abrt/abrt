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

TEST="dbus-api"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlAssert0 "No prior crashes should exist" $(abrt-cli list | wc -l)
        if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
            rlDie "Won't proceed"
        fi

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        rlRun "dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetProblems &> dbus_reply.log"

        rlAssertGrep "array" dbus_reply.log
        rlAssertGrep "string" dbus_reply.log
        rlAssertGrep "$crash_PATH" dbus_reply.log

        rlLog "Generate second crash"
        top -b > /dev/null &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Second crash recorded" $(abrt-cli list | wc -l) 0
        crash2_PATH="$(abrt-cli list -f | grep Directory \
            | grep -v "$crash_PATH" \
            | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash2_PATH" ]; then
            rlDie "No crash dir generated for second crash, this shouldn't happen"
        fi

        rlRun "dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetProblems &> dbus_second_reply.log"

        rlAssertGrep "array" dbus_second_reply.log
        rlAssertGrep "$crash_PATH" dbus_second_reply.log
        rlAssertGrep "$crash2_PATH" dbus_second_reply.log
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlRun "abrt-cli rm $crash2_PATH" 0 "Remove second crash directory"
        rlBundleLogs abrt *.log
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
