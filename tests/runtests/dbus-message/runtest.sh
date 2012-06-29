#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-message
#   Description: Verify that dbus messages are sent correctly
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

TEST="dbus-message"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlAssert0 "No prior crashes recorded" $(abrt-cli list | wc -l)
        if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
            rlDie "Won't proceed"
        fi

        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlRun "dbus-monitor --system \
            \"type='signal',interface='com.redhat.abrt',path='/com/redhat/abrt'\" \
            > dbus.log &" 0 "Running dbus-monitor"
    rlPhaseEnd

    rlPhaseStartTest
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %2
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        package="$(abrt-cli list -f | grep -i Package | awk '{ print $2 }' | tail -n1)"
        user="$( id -u )"

        # have to wait long period because of a snail post-create event
        sleep 150
        kill %1 || kill -9 %1 # kill dbus-monitor

        rlAssertGrep "member=Crash" dbus.log
        rlAssertGrep "$package" dbus.log
        rlAssertGrep "$user" dbus.log
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlBundleLogs abrt 'dbus.log'
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
