#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-argument-validation
#   Description: Verify D-Bus methods validating arguments
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

TEST="dbus-argument-validation"
PACKAGE="abrt"
MALICIOUS_ELEMENTS=".. ../../ ../../../malicious /foo ."
MALICIOUS_PROBLEMS="./ .. ../../ /var/log /var/spool/abrt/../ /var/spool/abrt/. /var/spool/abrt../../  /var/spool/abrt.. /var/spool/abrt/../../var"

function test_malicious_problems
{
    rlLog "Malicious Problems"
    for banned in $MALICIOUS_PROBLEMS; do
        RES=$(dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.$1 \
                        string:"$banned" $2 $3 2>&1)
        rlAssertEquals "Refused to read invalid problem" "_$RES" "_Error org.freedesktop.problems.InvalidProblemDir: '$banned' is not a valid problem directory"
    done
}

function test_malicious_elements
{
    rlLog "Malicious Elements"
    for banned in $MALICIOUS_ELEMENTS; do
        RES=$(dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.$1 \
                        string:"$crash_PATH" string:"$banned" $2 2>&1)
        rlLog "$RES"
        rlAssertEquals "Detected malicious content" "_$RES" "_Error org.freedesktop.problems.InvalidElement: '$banned' is not a valid element name"
    done
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        rlRun "useradd -c \"dbus-NewProblem test user\" -M abrtdbustest"
        TEST_UID=`id -u abrtdbustest`
    rlPhaseEnd

    rlPhaseStartTest "NewProblem"
        rlLog "Not allowed characters in element name"
        for banned in $MALICIOUS_ELEMENTS; do
            rlLog "Contents of analyzer"
            RES=$(dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                        dict:string:string:"analyzer","$banned","executable","$(which true)","uuid","1" 2>&1)
            rlAssertEquals "Problem wasn't created" "x$RES" "xError org.freedesktop.problems.Failure: Cannot create a new problem"
            rlAssertEquals "No problem created" "_$(abrt-cli list 2> /dev/null | wc -l)" "_0"

            rlLog "Contents of type"
            RES=$(dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                        dict:string:string:"type","$banned","executable","$(which true)","uuid","1" 2>&1)
            rlAssertEquals "Problem wasn't created" "x$RES" "xError org.freedesktop.problems.Failure: Cannot create a new problem"
            rlAssertEquals "No problem created" "_$(abrt-cli list 2> /dev/null | wc -l)" "_0"

            prepare

            rlLog "Regular elements"
            SINCE=$(date +"%Y-%m-%d %T")
            RES=$(dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                        dict:string:string:"type","libreport","executable","$(which true)","uuid","81680083","$banned","content" 2>&1)
            rlRun "journalctl SYSLOG_IDENTIFIER=dbus-daemon SYSLOG_IDENTIFIER=org.freedesktop.problems --since=\"$SINCE\" | grep \"Problem data field name contains disallowed chars: '$banned'\""

            wait_for_hooks

            problem_ID=$(echo $RES | sed -n 's/^.*\s\+string "\(\/var\/.*\/abrt\/libreport-.*\)"\s*$/\1/p')
            rlRun "abrt-cli rm $problem_ID"
        done

        rlLog "Non-root users is not allowed to create CCpp, Kerneloops, VMCore, Xorg"
        for banned in "CCpp" "Kerneloops" "vmcore" "xorg"; do
            RES=$(su abrtdbustest -c "dbus-send --system --type=method_call --print-reply \
                        --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
                        dict:string:string:\"type\",\"$banned\",\"executable\",\"$(which true)\",\"uid\",\"3\"" 2>&1)
            rlAssertEquals "Correct error message" "x$RES" "xError org.freedesktop.problems.Failure: You are not allowed to create element 'type' containing '$banned'"
            rlAssertEquals "No problem created" "_$(abrt-cli list 2> /dev/null | wc -l)" "_0"
            sleep 1
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "userdel -r -f abrtdbustest"
    rlPhaseEnd

    rlPhaseStartSetup
        check_prior_crashes
        generate_crash
        wait_for_hooks
        get_crash_path
    rlPhaseEnd

    rlPhaseStartTest "GetInfo"
        test_malicious_problems GetInfo "array:string:\"type\""

        rlLog "Malicious Elements"
        for banned in $MALICIOUS_ELEMENTS; do
            RES=$(dbus-send --system --type=method_call --print-reply \
                            --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetInfo \
                            string:"$crash_PATH" array:string:"$banned" 2>&1 | tr '\n' ' ')
            rlLog "$RES"
            rlRun "echo \"$RES\" | grep 'array \[    \]'"
        done
    rlPhaseEnd

    rlPhaseStartTest "SetElement"
        test_malicious_problems SetElement "string:\"foo\"" "string:\"blah\""
        test_malicious_elements SetElement "string:\"blah\""
    rlPhaseEnd

    rlPhaseStartTest "DeleteElement"
        test_malicious_problems DeleteElement "string:\"foo\""
        test_malicious_elements DeleteElement
    rlPhaseEnd

    rlPhaseStartTest "TestElementExists"
        test_malicious_problems TestElementExists "string:\"foo\""
        test_malicious_elements TestElementExists
    rlPhaseEnd

    rlPhaseStartTest "GetProblemData"
        test_malicious_problems GetProblemData
    rlPhaseEnd

    rlPhaseStartTest "ChownProblemDir"
        test_malicious_problems ChownProblemDir
    rlPhaseEnd

    rlPhaseStartTest "DeleteProblem"
        test_malicious_problems ChownProblemDir
    rlPhaseEnd

    rlPhaseStartTest "FindProblemByElementInTimeRange"
        rlLog "Malicious Elements"
        for banned in $MALICIOUS_ELEMENTS; do
            RES=$(dbus-send --system --type=method_call --print-reply \
                            --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.FindProblemByElementInTimeRange \
                            string:"$banned" string:"foo" int64:0 int64:1 boolean:false 2>&1 | tr '\n' ' ')
            rlLog "$RES"
            rlAssertEquals "Detected malicious content" "_$RES" "_Error org.freedesktop.problems.InvalidElement: '$banned' is not a valid element name "
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
