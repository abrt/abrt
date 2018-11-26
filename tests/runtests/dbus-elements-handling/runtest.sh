#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-elements-handling
#   Description: Check D-Bus add/remove elements and change element values methods
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

TEST="dbus-elements-handling"
PACKAGE="abrt"
STAT="stat --format=%A,%U,%G"

function abrtDBusNewProblem() {
    args=analyzer,libreport,executable,$(which true),uuid,$(date +%s.%N)

    if [ -n "$2" ]; then
        args = $args,$2
    fi

    dbus-send --system --type=method_call --print-reply \
              --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.NewProblem \
              dict:string:string:$args 2>&1 | tail -1 | sed 's/ *string *"\(.*\)"/\1/'
}

function abrtDBusSetElement() {
    dbus-send --system --type=method_call --print-reply \
              --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.SetElement \
              string:$1 string:$2 string:$3 2>&1 | awk -v first=1 '{ if (first) { first=0 } else { print } } /Error/ { print }'
}

function abrtDBusDelElement() {
    dbus-send --system --type=method_call --print-reply \
              --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.DeleteElement \
              string:$1 string:$2 2>&1 | awk -v first=1 '{ if (first) { first=0 } else { print} } /Error/ { print }'
}

function abrtDBusGetElement() {
    dbus-send --system --type=method_call --print-reply \
              --dest=org.freedesktop.problems /org/freedesktop/problems org.freedesktop.problems.GetInfo \
              string:$1 array:string:$2 2>&1 | awk -v elem=$2 -F\" '/string/ { if (beg) { print $2; exit } } $0 ~ "string \""elem { beg=1 }'
}

# $1 problem directory
# $2 user name
# $3 expected response for new
# $4 expected response for change
# $5 expected response for delete
# $6 expected response for delete nonexisting
function abrtElementsHandlingTest() {
    # the time element is required, it must be there
    problem_stat=`$STAT $1/time`

    rlAssertEquals "A new element is not created yet" "_$(abrtDBusGetElement $1 the_element)" "_"
    res=$(su $2 -c "abrtDBusSetElement $1 the_element an_elements_value")
    rlAssertEquals "SetElement method for nonexisting element" "_$res" "_$3"
    if [ -n "$3" ]; then
        rlAssertEquals "Failed to create the new element" "_$(abrtDBusGetElement $1 the_element)" "_"
        rlAssertEquals "SetElement method for nonexisting element as root" "_$(abrtDBusSetElement $1 the_element an_elements_value)" "_"
    else
        rlAssertExists $1/the_element
    fi
    rlAssertEquals "The element has been created with a passed value" "_$(abrtDBusGetElement $1 the_element)" "_an_elements_value"
    rlAssertEquals "The new element has correct rights" "_$($STAT $1/the_element)" "_$problem_stat"

    if [ ! -e $1/the_element ]; then
        rlLog "Create missing element manually"
        rlRun "echo -n 'the_element' > $1/the_element" 0 "Add element"
        rlRun "chmod `stat --format=%a $1/time` $1/the_element" 0 "Set correct perms"
        rlRun "chown `stat --format=%U:%G $1/time` $1/the_element" 0 "Set correct user and group"
    fi

    res=$(su $2 -c "abrtDBusSetElement $1 the_element the_fifth_element")
    rlAssertEquals "SetElement method for existing element" "_$res" "_$4"
    if [ -n "$4" ]; then
        rlAssertEquals "Failed to update the element" "_$(abrtDBusGetElement $1 the_element)" "_an_elements_value"
        rlAssertEquals "SetElement method for existing element as root" "_$(abrtDBusSetElement $1 the_element the_fifth_element)" "_"
    fi
    rlAssertEquals "A value of the updated element has the_element" "_$(abrtDBusGetElement $1 the_element)" "_the_fifth_element"
    rlAssertEquals "The updated element has correct rights" "_$($STAT $1/the_element)" "_$problem_stat"

    res=$(su $2 -c "abrtDBusDelElement $1 the_element")
    rlAssertEquals "DelElement method for existing element" "_$res" "_$5"
    if [ -n "$5" ]; then
        rlAssertEquals "Failed to delete the element" "_$(abrtDBusGetElement $1 the_element)" "_the_fifth_element"
        rlAssertEquals "DelElement method for existing element as root" "_$(abrtDBusDelElement $1 the_element)" "_"
    else
        rlAssertNotExists $1/the_element
    fi
    res=$(su $2 -c "abrtDBusDelElement $1 the_element")
    rlAssertEquals "DelElement method for nonexisting element" "_$res" "_$6"

    old_uid=$(abrtDBusGetElement $1 uid)
    res=$(su $2 -c "abrtDBusDelElement $1 uid")
    rlAssertEquals "DelElement method for UID" "_$res" "_Error org.freedesktop.problems.ProtectedElement: 'uid' element can't be modified"
    rlAssertEquals "The UID element is unchanged after delete attempt" "_$(abrtDBusGetElement $1 uid)" "_$old_uid"

    new_uid=$(id -u abrtdbustestanother)
    res=$(su $2 -c "abrtDBusSetElement $1 uid $new_uid")
    rlAssertEquals "SetElement method for UID element" "_$res" "_Error org.freedesktop.problems.ProtectedElement: 'uid' element can't be modified"
    rlAssertEquals "The UID element is unchanged after set attempt" "_$(abrtDBusGetElement $1 uid)" "_$old_uid"

    old_time=$(abrtDBusGetElement $1 time)
    res=$(su $2 -c "abrtDBusDelElement $1 time")
    rlAssertEquals "DelElement method for TIME" "_$res" "_Error org.freedesktop.problems.ProtectedElement: 'time' element can't be modified"
    rlAssertEquals "The TIME element is unchanged after delete attempt" "_$(abrtDBusGetElement $1 time)" "_$old_time"

    new_time=$((old_time + 10))
    res=$(su $2 -c "abrtDBusSetElement $1 time $new_time")
    rlAssertEquals "SetElement method for TIME element" "_$res" "_Error org.freedesktop.problems.ProtectedElement: 'time' element can't be modified"
    rlAssertEquals "The TIME element is unchanged after set attempt" "_$(abrtDBusGetElement $1 time)" "_$old_time"
}

function abrtMaxCrashReportsSizeTest() {
    rlRun "resp=\$(./set_element.py $1 onemibofx 1 1024)"
    rlAssertEquals "No free space detected" "_$resp" "_org.freedesktop.problems.Failure: No problem space left"

    rlRun "resp=\$(./set_element.py $1 onemibofx 2 512)"
    rlAssertEquals "Size limit correctly checks the size limit according to a new size of an element" "_$resp" "_"
}

rlJournalStart
    rlPhaseStartSetup
        rlRun "useradd -c \"dbus-elements-handling test an unprivileged user\" -M abrtdbustestone" 0 "Create a test user"
        rlRun "useradd -c \"dbus-elements-handling test an another user\" -M abrtdbustestanother" 0 "Create an another test user"
        export -f abrtDBusNewProblem
        export -f abrtDBusSetElement
        export -f abrtDBusDelElement
        export -f abrtDBusGetElement
        # Set limit to 1Midk
        rlRun "OLDCRASHSIZE=\"$(augtool print /files/etc/abrt/abrt.conf/MaxCrashReportsSize | tr -d '=\"')\"" 0 "Create a backup of abrt configuration"
        rlRun "augtool set /files/etc/abrt/abrt.conf/MaxCrashReportsSize 1" 0 "Set limit for crash reports to 1MiB"

        # set only if option PrivateReports exists
        grep -q PrivateReports /etc/abrt/abrt.conf && \
        old_private_reports_value=`augtool get /files/etc/abrt/abrt.conf/PrivateReports | cut -d'=' -f2` && \
        rlRun "augtool set /files/etc/abrt/abrt.conf/PrivateReports no" 0 "Set PrivateReports to no"

        rlRun "systemctl restart abrtd.service" 0 "Restart abrt service"

        load_abrt_conf
        prepare
        rlLog "Create a problem data as the root user"
        roots_problem=`abrtDBusNewProblem deleted,to_be_deleted,changed,to_be_changed`
        if echo $roots_problem | grep -s "org.freedesktop.problems.Failure"; then
          rlDie "Create problem failed"
        fi

        wait_for_hooks
        roots_problem_path="$(abrt-cli list $ABRT_CONF_DUMP_LOCATION | awk -v id=$roots_problem '$0 ~ "Directory:.*"id { print $2 }')"
        if [ -z "$roots_problem_path" ]; then
            rlDie "Not found path"
        fi

        rlRun "rm -f $roots_problem_path/sosreport.tar.*"

        prepare
        rlLog "Create a problem data as the unprivileged user"
        unprivilegeds_problem=`su abrtdbustestone -c 'abrtDBusNewProblem deleted,to_be_deleted,changed,to_be_changed'`
        if echo $unprivilegeds_problem | grep -s "org.freedesktop.problems.Failure"; then
          rlDie "Create problem failed"
        fi

        wait_for_hooks
        unprivilegeds_problem_path="$(abrt-cli list $ABRT_CONF_DUMP_LOCATION | awk -v id=$unprivilegeds_problem '$0 ~ "Directory:.*"id { print $2 }')"
        if [ -z "$unprivilegeds_problem_path" ]; then
            rlDie "Not found path"
        fi

        rlRun "rm -f $unprivilegeds_problem_path/sosreport.tar.*"
    rlPhaseEnd

    rlPhaseStartTest "Sanity tests"
        rlAssertEquals "SetElement method for an invalid problem id" "_$(abrtDBusSetElement foo the_element a_value)" "_Error org.freedesktop.problems.InvalidProblemDir: 'foo' is not a valid problem directory"
        rlAssertEquals "DelElement method for an invalid problem id" "_$(abrtDBusDelElement foo the_element)" "_Error org.freedesktop.problems.InvalidProblemDir: 'foo' is not a valid problem directory"

        rlAssertEquals "Can't create an element with empty name" "_$(abrtDBusSetElement  $roots_problem_path '' some_value)" "_Error org.freedesktop.problems.InvalidElement: '' is not a valid element name"
        rlAssertEquals "Can't create an element with too long name" "_$(abrtDBusSetElement  $roots_problem_path 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' some_value)" "_Error org.freedesktop.problems.InvalidElement: 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' is not a valid element name"
    rlPhaseEnd

    rlPhaseStartTest "Handle elements as root"
               # Test functionality as the root user
        rlLog "root changes root's problem"
        abrtElementsHandlingTest "$roots_problem_path" "root"

        rlLog "root changes user's problem"
        abrtElementsHandlingTest "$unprivilegeds_problem_path" "root"

        rlLog "Chech max crash reports size"
        abrtMaxCrashReportsSizeTest "$roots_problem_path" "root"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $roots_problem_path" 0 "Remove roots crash directory"
        rlRun "abrt-cli rm $unprivilegeds_problem_path" 0 "Remove users crash directory"
    rlPhaseEnd

    rlPhaseStartSetup
        rlRun "systemctl stop abrtd.service" 0 "Stop abrtd before cleaning of the dump location"
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/*" 0 "Clean the dump location"
        rlRun "systemctl start abrtd.service" 0 "Start abrtd after cleaning of the dump location"

        prepare
        rlLog "Create a problem data as the root user"
        roots_problem=`abrtDBusNewProblem`
        if echo $roots_problem | grep -s "org.freedesktop.problems.Failure"; then
          rlDie "Create problem failed"
        fi

        wait_for_hooks
        roots_problem_path="$(abrt-cli list $ABRT_CONF_DUMP_LOCATION | awk -v id=$roots_problem '$0 ~ "Directory:.*"id { print $2 }')"
        if [ -z "$roots_problem_path" ]; then
            rlDie "Not found path problem path"
        fi

        rlRun "rm -f $roots_problem_path/sosreport.tar.*"

        prepare
        rlLog "Create a problem data as the unprivileged user"
        unprivilegeds_problem=`su abrtdbustestone -c 'abrtDBusNewProblem'`
        if echo $unprivilegeds_problem | grep -s "org.freedesktop.problems.Failure"; then
          rlDie "Create problem failed"
        fi

        wait_for_hooks
        unprivilegeds_problem_path="$(abrt-cli list $ABRT_CONF_DUMP_LOCATION | awk -v id=$unprivilegeds_problem '$0 ~ "Directory:.*"id { print $2 }')"
        if [ -z "$unprivilegeds_problem_path" ]; then
            rlDie "Not found path problem path"
        fi

        rlRun "rm -f $unprivilegeds_problem_path/sosreport.tar.*"

        prepare
        rlLog "Create a problem data as the unprivileged user"
        second_unprivilegeds_problem=`su abrtdbustestone -c 'abrtDBusNewProblem'`
        if echo $second_unprivilegeds_problem | grep -s "org.freedesktop.problems.Failure"; then
          rlDie "Create problem failed"
        fi

        wait_for_hooks
        second_unprivilegeds_problem_path="$(abrt-cli list $ABRT_CONF_DUMP_LOCATION | awk -v id=$second_unprivilegeds_problem '$0 ~ "Directory:.*"id { print $2 }')"
        if [ -z "$second_unprivilegeds_problem_path" ]; then
            rlDie "Not found path problem path"
        fi

        rlRun "rm -f $second_unprivilegeds_problem_path/sosreport.tar.*"
    rlPhaseEnd

    rlPhaseStartTest "Handle elements as a user"
        rlLog "User changes root's problem"
        abrtElementsHandlingTest "$roots_problem_path" "abrtdbustestone" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized"

        rlLog "User changes user's problem"
        abrtElementsHandlingTest "$unprivilegeds_problem_path" "abrtdbustestone"

        rlLog "Another user changes user's problem"
        abrtElementsHandlingTest "$second_unprivilegeds_problem_path" "abrtdbustestanother" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized" \
            "Error org.freedesktop.problems.AuthFailure: Not Authorized"

        rlLog "Chech max crash reports size"
        abrtMaxCrashReportsSizeTest "$unprivilegeds_problem_path" "abrtdbustestone"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "userdel -r -f abrtdbustestone" 0 "Remove the test user"
        rlRun "userdel -r -f abrtdbustestanother" 0 "Remove the another test user"

        # set only if option PrivateReports exists
        grep -q PrivateReports /etc/abrt/abrt.conf && \
        rlRun "augtool set /files/etc/abrt/abrt.conf/PrivateReports $old_private_reports_value" 0 "Set PrivateReports to yes"

        if [ -n "$OLDCRASHSIZE" ]; then
            rlRun "augtool set $OLDCRASHSIZE" 0
        else
            rlRun "augtool rm /files/etc/abrt/abrt.conf/MaxCrashReportsSize" 0
        fi
        rlRun "systemctl restart abrtd.service" 0 "Restart abrtd after configuration changes"
        rlRun "rm -rf -- $ABRT_CONF_DUMP_LOCATION/*"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
