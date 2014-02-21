#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of event-configuration
#   Description: Check functions for reading event configuration
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

TEST="event-configuration"
PACKAGE="abrt"

function prepare_event
{
    t_XML_FILE="${!var_name}.xml"
    rlRun "eval ${var_name}_XML_FILE=\"$t_XML_FILE\""

    t_XML_FILE_SYSTEM="/usr/share/libreport/events/$t_XML_FILE"
    rlRun "eval ${var_name}_XML_FILE_SYSTEM=\"$t_XML_FILE_SYSTEM\""

    t_CONF_FILE="${!var_name}.conf"
    rlRun "eval ${var_name}_CONF_FILE=\"$t_CONF_FILE\""

    t_CONF_FILE_SYSTEM="/etc/libreport/events/$t_CONF_FILE"
    rlRun "eval ${var_name}_CONF_FILE_SYSTEM=\"$t_CONF_FILE_SYSTEM\""

    t_DEF_FILE="event_${!var_name}.conf"
    rlRun "eval ${var_name}_DEF_FILE=\"$t_DEF_FILE\""

    t_DEF_FILE_SYSTEM="/etc/libreport/events.d/$t_DEF_FILE"
    rlRun "eval ${var_name}_DEF_FILE_SYSTEM=\"$t_DEF_FILE_SYSTEM\""

    t_CONF_FILE_USER="$TEST_USER_HOME_CONF/$t_CONF_FILE"
    rlRun "eval ${var_name}_CONF_FILE_USER=\"$t_CONF_FILE_USER\""

    t_CONF_FILE_XDG="$TEST_XDG_CACHE_HOME_CONF/$t_CONF_FILE"
    rlRun "eval ${var_name}_CONF_FILE_XDG=\"$t_CONF_FILE_XDG\""

    rlRun "cp $t_XML_FILE $t_XML_FILE_SYSTEM"
    rlRun "cp $t_DEF_FILE $t_DEF_FILE_SYSTEM"
    rlRun "rm -f $t_CONF_FILE_SYSTEM $t_CONF_FILE_USER $t_CONF_FILE_XDG"
}

rlJournalStart
    rlPhaseStartSetup
        rlRun "TEST_USER_HOME_CONF=\"$HOME/.cache/abrt/events\""
        rlRun "TEST_XDG_CACHE_HOME=\"$HOME/.utopia\""
        rlRun "TEST_XDG_CACHE_HOME_CONF=\"$HOME/.utopia/abrt/events\""

        rlRun "TEST_EVENT=\"test_Configuration\""
        var_name="TEST_EVENT"
        prepare_event

        rlRun "GENERATED_FILE=\"/tmp/${TEST_EVENT_CONF_FILE}\""

        rlRun "TEST_INCLUDE_EVENT=\"test_IncludesConfiguration\""
        var_name="TEST_INCLUDE_EVENT"
        prepare_event

        rlRun "INCLUDE_GENERATED_FILE=\"/tmp/${TEST_INCLUDE_EVENT_CONF_FILE}\""

        prepare
        generate_crash
        get_crash_path
        wait_for_hooks
    rlPhaseEnd

    rlPhaseStartTest "Default values"
        rlRun "rm -rf $GENERATED_FILE"
        rlRun "report-cli -e $TEST_EVENT -- $crash_PATH"
        rlAssertNotDiffer $TEST_EVENT_CONF_FILE $GENERATED_FILE
        rlRun "mv $GENERATED_FILE default_gen_$TEST_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartTest "System configuration"
        rlRun "rm -rf $GENERATED_FILE"
        rlRun "cp system_$TEST_EVENT_CONF_FILE $TEST_EVENT_CONF_FILE_SYSTEM"
        rlRun "report-cli -e $TEST_EVENT -- $crash_PATH"
        rlAssertNotDiffer system_result_$TEST_EVENT_CONF_FILE $GENERATED_FILE
        rlRun "mv $GENERATED_FILE sytem_gen_$TEST_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartTest "User configuration"
        rlRun "rm -rf $GENERATED_FILE"
        rlRun "mkdir -p $TEST_USER_HOME_CONF"
        rlRun "cp user_$TEST_EVENT_CONF_FILE $TEST_EVENT_CONF_FILE_USER"
        rlRun "report-cli -e $TEST_EVENT -- $crash_PATH"
        rlAssertNotDiffer user_result_$TEST_EVENT_CONF_FILE $GENERATED_FILE
        rlRun "mv $GENERATED_FILE user_gen_$TEST_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartTest "Modified XDG_CACHE_HOME configuration"
        rlRun "rm -rf $GENERATED_FILE"
        rlRun "OLD_XDG_CACHE_HOME=\"$XDG_CACHE_HOME\""
        rlRun "export XDG_CACHE_HOME=\"$TEST_XDG_CACHE_HOME\""
        rlRun "mkdir -p $TEST_XDG_CACHE_HOME_CONF"
        rlRun "cp xdg_$TEST_EVENT_CONF_FILE $TEST_EVENT_CONF_FILE_XDG"
        rlRun "report-cli -e $TEST_EVENT -- $crash_PATH"
        rlRun "export XDG_CACHE_HOME=\"$OLD_XDG_CACHE_HOME\""
        rlAssertNotDiffer xdg_result_$TEST_EVENT_CONF_FILE $GENERATED_FILE
        rlRun "mv $GENERATED_FILE xdg_gen_$TEST_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartTest "Options included from another event"
        rlRun "rm -rf $INCLUDE_GENERATED_FILE"
        rlRun "report-cli -e $TEST_INCLUDE_EVENT -- $crash_PATH"
        rlAssertNotDiffer $TEST_INCLUDE_EVENT_CONF_FILE $INCLUDE_GENERATED_FILE
        rlRun "mv $INCLUDE_GENERATED_FILE default_gen_$TEST_INCLUDE_EVENT_CONF_FILE"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -f $TEST_EVENT_XML_FILE_SYSTEM $TEST_EVENT_DEF_FILE_SYSTEM"
        rlRun "rm -f ${TEST_EVENT_CONF_FILE_SYSTEM} ${TEST_EVENT_CONF_FILE_USER} ${TEST_EVENT_CONF_FILE_XDG}"

        rlRun "rm -f $TEST_INCLUDE_EVENT_XML_FILE_SYSTEM $TEST_INCLUDE_EVENT_DEF_FILE_SYSTEM"
        rlRun "rm -f ${TEST_INCLUDE_EVENT_CONF_FILE_SYSTEM} ${TEST_INCLUDE_EVENT_CONF_FILE_USER} ${TEST_INCLUDE_EVENT_CONF_FILE_XDG}"

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
