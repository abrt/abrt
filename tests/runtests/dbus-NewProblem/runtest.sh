#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-NewProblem
#   Description: Check D-Bus NewProblem() method functionality
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2012 Red Hat, Inc. All rights reserved.
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

TEST="dbus-NewProblem"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        rlRun "useradd -c \"dbus-NewProblem test user\" -M abrtdbustest"
        TEST_UID=`id -u abrtdbustest`
    rlPhaseEnd

    rlPhaseStartTest
        rlLog "Create problem data as root without UID"
        dbus-send --system --type=method_call --print-reply \
                    --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                    dict:string:string:"analyzer","libreport","executable","$(which true)","uuid","1" > dbus_first_reply.log
        rlAssertGrep "^[ ]*string[ ]\+\"[^ ]\+\"$" dbus_first_reply.log
        problem_ID1=`cat dbus_first_reply.log | tail -1 | sed 's/ *string *"\(.*\)"/\1/'`

        wait_for_hooks

        rlAssertGreater "Problem data recorded" $(abrt-cli list | grep -c ${problem_ID1}) 0

        problem_PATH1="$(abrt-cli list | awk -v id=$problem_ID1 '$0 ~ "Directory:.*"id { print $2 }')"
        if [ ! -d "$problem_PATH1" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        problem_UID1="$(abrt-cli list | awk -v id=$problem_ID1 '$0 ~ "Directory:.*"id, /uid:/ { if ($1 == "uid:") { print $2 } else if ($1 == "") { print "missing uid"; exit } }')"
        rlAssertEquals "Problem uid is equal to 0" "0" "$problem_UID1"

        prepare

        rlLog "Create problem data as root with UID"
        dbus-send --system --type=method_call --print-reply \
                    --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                    dict:string:string:"analyzer","libreport","executable","$(which true)","uid","$TEST_UID","uuid","2" > dbus_second_reply.log
        rlAssertGrep "^[ ]*string[ ]\+\"[^ ]\+\"$" dbus_second_reply.log
        problem_ID2=`cat dbus_second_reply.log | tail -1 | sed 's/ *string *"\(.*\)"/\1/'`

        wait_for_hooks

        rlAssertGreater "Problem data recorded" $(abrt-cli list | grep -c ${problem_ID2}) 0

        problem_PATH2="$(abrt-cli list | awk -v id=${problem_ID2} '$0 ~ "Directory:.*"id { print $2 }')"
        if [ ! -d "$problem_PATH2" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        problem_UID2="$(abrt-cli list | awk -v id=$problem_ID2 '$0 ~ "Directory:.*"id, /uid:/ { if ($1 == "uid:") { print $2 } else if ($1 == "") { print "missing uid"; exit } }')"
        rlAssertEquals "Problem uid equals to the passed uid" "$TEST_UID" "$problem_UID2"

        prepare

        rlLog "Create problem data as a user without UID"
        su abrtdbustest -c 'dbus-send --system --type=method_call --print-reply \
                    --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                    dict:string:string:"analyzer","libreport","executable","$(which true)","uid","3"' > dbus_third_reply.log
        rlAssertGrep "^[ ]*string[ ]\+\"[^ ]\+\"$" dbus_third_reply.log
        problem_ID3=`cat dbus_third_reply.log | tail -1 | sed 's/ *string *"\(.*\)"/\1/'`

        wait_for_hooks

        rlAssertGreater "Problem data recorded" $(abrt-cli list | grep -c ${problem_ID3}) 0

        problem_PATH3="$(abrt-cli list | awk -v id=$problem_ID3 '$0 ~ "Directory:.*"id { print $2 }')"
        if [ ! -d "$problem_PATH3" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        problem_UID3="$(abrt-cli list | awk -v id=$problem_ID3 '$0 ~ "Directory:.*"id, /uid:/ { if ($1 == "uid:") { print $2 } else if ($1 == "") { print "missing uid"; exit } }')"
        rlAssertEquals "Problem uid equals to caller's uid" "$TEST_UID" "$problem_UID3"

        prepare

        rlLog "Create problem data as a user with root's UID"
        su abrtdbustest -c 'dbus-send --system --type=method_call --print-reply \
                    --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                    dict:string:string:"analyzer","libreport","executable","$(which true)","uid","0","uuid","4"' > dbus_fourth_reply.log
        rlAssertGrep "^[ ]*string[ ]\+\"[^ ]\+\"$" dbus_fourth_reply.log
        problem_ID4=`cat dbus_fourth_reply.log | tail -1 | sed 's/ *string *"\(.*\)"/\1/'`

        wait_for_hooks

        rlAssertGreater "Problem data recorded" $(abrt-cli list | grep -c ${problem_ID4}) 0

        problem_PATH4="$(abrt-cli list | awk -v id=$problem_ID4 '$0 ~ "Directory:.*"id { print $2 }')"
        if [ ! -d "$problem_PATH4" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi

        problem_UID4="$(abrt-cli list | awk -v id=$problem_ID4 '$0 ~ "Directory:.*"id, /uid:/ { if ($1 == "uid:") { print $2 } else if ($1 == "") { print "missing uid"; exit } }')"
        rlAssertEquals "Passed uid is replaced by caller's uid" "$TEST_UID" "$problem_UID4"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "userdel -r -f abrtdbustest"
        rlRun "abrt-cli rm $problem_PATH1" 0 "Remove crash directory"
        rlRun "abrt-cli rm $problem_PATH2" 0 "Remove crash directory"
        rlRun "abrt-cli rm $problem_PATH3" 0 "Remove crash directory"
        rlRun "abrt-cli rm $problem_PATH4" 0 "Remove crash directory"
        rlBundleLogs abrt *.log
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
