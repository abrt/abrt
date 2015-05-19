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

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel
        load_abrt_conf
        CRASHDIR=$(cat /etc/abrt/abrt.conf | grep DumpLocation | sed 's/.* = //')
    rlPhaseEnd

    rlPhaseStartTest
        prepare

        rlLog "Creating crash data."
        rlRun "sleep 1000 &" 0 "Running 'sleep' process"
        rlRun "kill -s SIGSEGV %%" 0 "Kill running process"

        wait_for_hooks

        rlRun "cd $CRASHDIR/ccpp*"

        rlLog "check if we are running a compatible systemd version"
        if ! journalctl --system -n1 >/dev/null
        then
            rlLog "journald does not have '--system' argument, using /var/log/messages instead"
            rlAssertExists var_log_messages
            rlAssertNotGrep "System Logs" var_log_messages
            rlAssertGrep "sleep" var_log_messages
        else
            rlAssertExists var_log_messages
            rlAssertGrep "System Logs" var_log_messages
            rlAssertGrep "sleep" var_log_messages
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf $CRASHDIR/ccpp*" 0 "Removing problem dirs"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
