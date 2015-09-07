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
CFG_FILE_UNPACKAGED="/etc/abrt/abrt-action-save-package-data.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)

        rlRun "cp will_segfault_in_new_pid.c verify_core_backtrace.py $TmpDir/"

        pushd $TmpDir

        rlFileBackup $CFG_FILE $CFG_FILE_UNPACKAGED
    rlPhaseEnd

    rlPhaseStartTest "CreateCoreBacktrace enabled"
        rlLogInfo "CreateCoreBacktrace = yes"
        rlRun "echo 'CreateCoreBacktrace = yes' > $CFG_FILE" 0 "Set CreateCoreBacktrace = yes"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "CreateCoreBacktrace enabled - New PID namespace"
        rlLogInfo "ProcessUnpackaged = yes"
        rlRun "augtool set /files/etc/abrt/$CFG_FILE_UNPACKAGED/ProcessUnpackaged yes" 0 "Set ProcessUnpackaged"

        # I did not use 'unshare --fork --pid will_segfault' because unshare
        # kills itself with the signal the child received.
        rlLogInfo "Build the binary"
        rlRun "gcc -std=gnu99 --pedantic -Wall -Wextra -Wno-unused-parameter -o will_segfault_in_new_pid will_segfault_in_new_pid.c"

        prepare
        rlRun "./will_segfault_in_new_pid"
        wait_for_hooks
        get_crash_path
        rlRun "killall abrt-hook-ccpp" 1 "Kill hung abrt-hook-ccpp process"

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertExists "$crash_PATH/coredump"
        rlAssertNotEquals "TID is the global TID" "_1" "_$(cat $crash_PATH/tid)"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "SaveFullCore disabled"
        rlLogInfo "CreateCoreBacktrace = yes"
        rlLogInfo "SaveFullCore = no"
        rlRun "echo 'CreateCoreBacktrace = yes' > $CFG_FILE" 0 "Set CreateCoreBacktrace = yes"
        rlRun "echo 'SaveFullCore = no' >> $CFG_FILE" 0 "Set SaveFullCore = no"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "glibc function on stack"
        rlLogInfo "Verify that glibc frames have resolved 'file_name' and 'build_id'"

        prepare
        sleep 81680083 &
        SLEEP_PID=$!
        sleep 1
        kill -SEGV $SLEEP_PID
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace" 0 "All frames must have required members"

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
