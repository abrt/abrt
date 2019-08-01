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
. ../aux/lib.sh

TEST="dbus-message"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlRun "dbus-monitor --system \
            \"type='signal',interface='org.freedesktop.problems',path='/org/freedesktop/problems'\" \
            > dbus.log &" 0 "Running dbus-monitor"
    rlPhaseEnd

    rlPhaseStartTest
        generate_crash
        wait_for_hooks
        get_crash_path

        package="$(abrt info | grep -i Package | awk '{ print $2 }')"
        user="$( id -u )"

        kill %1 || kill -9 %1 # kill dbus-monitor

        rlAssertGrep "member=Crash" dbus.log
        rlAssertGrep "$package" dbus.log
        rlAssertGrep "$user" dbus.log
    rlPhaseEnd

    rlPhaseStartCleanup
        remove_problem_directory
        rlBundleLogs abrt 'dbus.log'
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
