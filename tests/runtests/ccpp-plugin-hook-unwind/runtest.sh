#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-hook-unwind
#   Description: Tests ccpp-plugin's unwinding from core hook
#   Author: Richard Marko <rmarko@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="ccpp-plugin-hook-unwind"
PACKAGE="abrt"

CFG_FILE="/etc/abrt/plugins/CCpp.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlFileBackup $CFG_FILE
    rlPhaseEnd

    rlPhaseStartTest "CreateCoreBacktrace enabled"
        rlLogInfo "CreateCoreBacktrace = yes"
        rlRun "echo 'CreateCoreBacktrace = yes' > $CFG_FILE" 0 "Set CreateCoreBacktrace = yes"

        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertExists "$crash_PATH/coredump"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "SaveFullCore disabled"
        rlLogInfo "CreateCoreBacktrace = yes"
        rlLogInfo "SaveFullCore = no"
        rlRun "echo 'CreateCoreBacktrace = yes' > $CFG_FILE" 0 "Set CreateCoreBacktrace = yes"
        rlRun "echo 'SaveFullCore = no' >> $CFG_FILE" 0 "Set SaveFullCore = no"

        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore # CFG_FILE

        rlRun "ulimit -c $old_ulimit" 0

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
