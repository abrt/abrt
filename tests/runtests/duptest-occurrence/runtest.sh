#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of duptest-occurrence
#   Description: Tests if the last occurrence of a duplicate crash has been updated
#   Author: Petr Kubat <pkubat@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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

TEST="duptest-occurrence"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel
        load_abrt_conf
    rlPhaseEnd

    rlPhaseStartTest
        prepare

        rlLog "Creating problem data."
        dbus-send --system --type=method_call --print-reply \
          --dest=org.freedesktop.problems /org/freedesktop/problems \
          org.freedesktop.problems.NewProblem \
          dict:string:string:analyzer,libreport,reason,"Testing crash",backtrace,"die()",executable,"/usr/bin/true"

        wait_for_hooks

        rlRun "cd $ABRT_CONF_DUMP_LOCATION/libreport*"

        first_occurrence=`cat last_occurrence`

        rlLog "Ensure that the second crash will not occur at the same second as the first one"
        sleep 2

        prepare

        rlLog "Creating problem data a second time."
        dbus-send --system --type=method_call --print-reply \
          --dest=org.freedesktop.problems /org/freedesktop/problems \
          org.freedesktop.problems.NewProblem \
          dict:string:string:analyzer,libreport,reason,"Testing crash",backtrace,"die()",executable,"/usr/bin/true"

        wait_for_hooks

        rlAssertNotEquals "Checking if last_occurrence has been updated" "_$first_occurrence" "_`cat last_occurrence`"
        rlAssertEquals "Checking if abrt counted multiple crashes" `cat count` 2
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/libreport*" 0 "Removing problem dir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
