#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of incomplete-problem
#   Description: Tests recognition of incomplete problem data.
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

TEST="incomplete-problem"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel
        load_abrt_conf
        CRASHDIR=$(cat /etc/abrt/abrt.conf | grep DumpLocation | sed 's/.* = //')
    rlPhaseEnd

    rlPhaseStartTest
        prepare

        rlLog "Creating problem data."
        dbus-send --system --type=method_call --print-reply \
          --dest=org.freedesktop.problems /org/freedesktop/problems \
          org.freedesktop.problems.NewProblem \
          dict:string:string:analyzer,libreport,reason,"Testing crash",backtrace,"die()",executable,"/usr/bin/true"

        wait_for_hooks

        rlRun "cd $CRASHDIR/libreport*"
        rlLog "Removing count file and restarting abrtd"
        rlRun "rm -f count"
        rlRun "systemctl restart abrtd"
        sleep 4s
        rlLog "Abrtd should recognize the problem and add a 'not-reportable file'"
        rlAssertExists not-reportable
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf $CRASHDIR/libreport*" 0 "Removing problem dir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
