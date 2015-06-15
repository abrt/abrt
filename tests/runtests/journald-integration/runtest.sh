#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of journald-integration
#   Description: Tests the integration of systemd logging to abrt
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

TEST="journald-integration"
PACKAGE="abrt"
CFG_FILE="/etc/abrt/abrt-action-save-package-data.conf"
EXE=morituri

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel

        rlFileBackup $CFG_FILE
        sed -i 's/\(ProcessUnpackaged\) = no/\1 = yes/g' $CFG_FILE

        TmpDir=$(mktemp -d)
        chmod a+rwx $TmpDir
        cp ${EXE}.c $TmpDir
        pushd $TmpDir
        rlRun "useradd abrtlogtest -M" 0

    rlPhaseEnd

    rlPhaseStartTest
        prepare

        rlRun "gcc -std=c99 morituri.c -o $EXE"

        rlLog "Creating crash data."
        rlRun "./$EXE" 134

        wait_for_hooks
        get_crash_path

        pushd $crash_PATH

        rlLog "check if we are running a compatible systemd version"
        if ! journalctl --system -n1 >/dev/null
        then
            rlLog "journald does not have '--system' argument, using /var/log/messages instead"
            rlAssertExists var_log_messages
            rlAssertNotGrep "System Logs" var_log_messages
            rlAssertGrep "sleep" var_log_messages
        else
            rlAssertExists var_log_messages
            rlAssertNotGrep "System Logs" var_log_messages
            rlAssertGrep "User Logs" var_log_messages
        fi

        LINE="./$EXE is on its way to die ..."
        rlRun "REL_LINES_CNT=`cat var_log_messages | grep -c \"$LINE\"`"
        if [ -z $REL_LINES_CNT ]; then
            rlFail "Failed to get relevant lines"
        else
            rlAssertGreaterOrEqual "Log lines count" "$REL_LINES_CNT" "7"
        fi

        popd

    rlPhaseEnd

    rlPhaseStartTest
        prepare

        SENSITIVE_LINE="a sesnsitive line containing $EXE"
        rlRun "logger '$SENSITIVE_LINE'"

        rlLog "Creating crash data."
        rlRun "su abrtlogtest -c ./$EXE" 134

        wait_for_hooks
        get_crash_path

        pushd $crash_PATH

        rlAssertNotGrep "System Logs" var_log_messages

        rlLog "check if we are running a compatible systemd version"
        if ! journalctl --system -n1 >/dev/null
        then
            rlLog "journald does not have '--system' argument, using /var/log/messages instead"
            rlAssertExists var_log_messages
            rlAssertGrep "sleep" var_log_messages
        else
            rlAssertExists var_log_messages
            rlAssertNotGrep "System Logs" var_log_messages
            rlAssertGrep "User Logs" var_log_messages
        fi

        rlAssertNotGrep "$SENSITIVE_LINE" var_log_messages

        LINE="./$EXE is on its way to die ..."
        rlRun "REL_LINES_CNT=`cat var_log_messages | grep -c \"$LINE\"`"
        if [ -z $REL_LINES_CNT ]; then
            rlFail "Failed to get relevant lines"
        else
            rlAssertGreaterOrEqual "Log lines count" "$REL_LINES_CNT" "7"
        fi

        popd

    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "userdel -r -f abrtlogtest" 0
        rlRun "abrt-cli rm $crash_PATH" 0 "Removing problem dirs"
        rlFileRestore
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
