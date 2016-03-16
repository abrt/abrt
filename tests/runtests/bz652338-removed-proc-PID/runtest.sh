#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz652338-removed-proc-PID
#   Description: Tests if abrt survives non-existent /proc/<PID>/ file
#   Author: Michal Nowak <mnowak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
#
#   This copyrighted material is made available to anyone wishing
#   to use, modify, copy, or redistribute it subject to the terms
#   and conditions of the GNU General Public License version 2.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE. See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public
#   License along with this program; if not, write to the Free
#   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Include rhts environment
. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="bz652338-removed-proc-PID"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
        core_pipe_limit_bkp="$(cat /proc/sys/kernel/core_pipe_limit)"
        rlRun "echo 1 > /proc/sys/kernel/core_pipe_limit" 0 "Set core_pipe_limit to 1 from previous $core_pipe_limit_bkp"
        rlRun "dnf install -y mlocate procps" 0 "Install locate, updatedb & top binaries"
    rlPhaseEnd

    rlPhaseStartTest
        CLI_LIST="abrt-cli list"
        CLI_RM="abrt-cli rm"

        tail -f -n 0 /var/log/messages > var-log-messages &

        sleep 2

        rlRun "sleep 99 &" 0 "Exec process #1"
        rlRun "sleep 100 &" 0 "Exec process #2"
        yes > /dev/null &
        updatedb &
        top -b > /dev/null &
        sleep 1

        rlRun "jobs 2>&1 | tee jobs.log" 0 "List background jobs"
        sleep 1

        killall -11 yes &
        killall -11 updatedb &
        killall -11 top &
        rlRun "kill -11 %2" 0 "Kill process #1"
        rlRun "kill -11 %3" 0 "Kill process #2"

        rlRun "$CLI_LIST 2>&1 | tee cli_list.log"
        kill %1 || kill -9 %1 # kill tailf

        wait_for_sosreport

        rlAssertGrep "Skipping core dump" var-log-messages

        rlRun "$CLI_LIST | grep package | grep abrt" 1 "abrtd did not crashed"

        dirs="$($CLI_LIST | grep Directory | awk '{ print $2 }')"
        for dir in $dirs; do
            rlRun "$CLI_RM $dir" 0 "Dump dir removed ($dir)"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "echo $core_pipe_limit_bkp > /proc/sys/kernel/core_pipe_limit" 0 "Restore core_pipe_limit back to $core_pipe_limit_bkp"
        rlBundleLogs abrt var-log-messages $(ls *.log)
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
