#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin
#   Description: Tests basic functionality of ccpp-plugin
#   Author: Richard Marko <rmarko@redhat.com>
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

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="ccpp-plugin"
PACKAGE="abrt"


rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        cp verify_core_backtrace.py verify_core_backtrace_length.py will_segfault_in_new_pid.c will_segfault_locked_memory.c $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin (testuser crash)"
        rlRun "useradd testuser" 0
        generate_crash testuser
        wait_for_hooks
        get_crash_path

        ARCHITECTURE=$(rlGetPrimaryArch)

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        rlAssertExists "$crash_PATH/uuid"

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertGrep "/bin/will_segfault" "$crash_PATH/core_backtrace"
    rlPhaseEnd

    rlPhaseStartTest "core_backtrace contents"
        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $ARCHITECTURE 2>&1 > verify_result" 0
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlRun "userdel -r -f testuser" 0
    rlPhaseEnd

    rlPhaseStartTest "core_backtrace for stack overflow"
        prepare
        generate_stack_overflow_crash
        wait_for_hooks
        get_crash_path

        check_dump_dir_attributes $crash_PATH

        rlAssertExists "$crash_PATH/core_backtrace"
        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $ARCHITECTURE 2>&1 > verify_result_overflow" 0
        rlRun "./verify_core_backtrace_length.py $crash_PATH/core_backtrace 2>&1 > verify_result_len_overflow" 0
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "auto-load GDB script from /var/cache/abrt-di/"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        check_dump_dir_attributes $crash_PATH

        rlRun "mkdir -p /var/cache/abrt-di/usr/lib/debug/usr/bin"

cat > /var/cache/abrt-di/usr/lib/debug/usr/bin/will_segfault-gdb.py <<EOF
#!/usr/bin/python
print "will_segfault auto-loaded python GDB script"
EOF

        rlAssertExists "/var/cache/abrt-di/usr/lib/debug/usr/bin/will_segfault-gdb.py"

        rlRun "abrt-action-generate-backtrace -d $crash_PATH"
        rlAssertExists "$crash_PATH/backtrace"
        rlAssertGrep "will_segfault auto-loaded python GDB script" "$crash_PATH/backtrace"
        rlAssertNotGrep "auto-loading has been declined by your" "$crash_PATH/backtrace"

        rlRun "rm /var/cache/abrt-di/usr/lib/debug/usr/bin/will_segfault-gdb.py"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "crash in a non-init PID NS"
        # I did not use 'unshare --fork --pid will_segfault' because unshare
        # kills itself with the signal the child received.
        rlLogInfo "Build the binary"
        rlRun "gcc -std=gnu99 --pedantic -Wall -Wextra -Wno-unused-parameter -o will_segfault_in_new_pid will_segfault_in_new_pid.c"

        prepare
        rlRun "./will_segfault_in_new_pid"
        wait_for_hooks
        get_crash_path

        rlRun "killall abrt-hook-ccpp" 1 "Kill hung abrt-hook-ccpp process"

        rlAssertExists "$crash_PATH/coredump"
        rlAssertExists "$crash_PATH/global_pid"
        rlAssertNotEquals "Global PID is sane" "_1" "_$(cat $crash_PATH/global_pid)"
        rlAssertEquals "PID from process' PID NS" "_1" "_$(cat $crash_PATH/pid)"
        rlAssertGrep "Name:[[:space:]]*will_segfault" $crash_PATH/proc_pid_status
        rlAssertGrep "will_segfault" $crash_PATH/cmdline

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "crash of a process with locked memory"
        EXECUTABLE=will_segfault_locked_memory
        rlLogInfo "Build the binary"
        rlRun "gcc -std=gnu99 --pedantic -Wall -Wextra -Wno-unused-parameter -o $EXECUTABLE $EXECUTABLE.c"

        prepare
        rlRun "./$EXECUTABLE" 134
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/not-reportable"
        rlAssertGrep "locked memory" "$crash_PATH/not-reportable"
        rlAssertEquals "Detected the right process" "_./$EXECUTABLE" "_$(cat $crash_PATH/cmdline)"
        rlRun "grep VmLck $crash_PATH/proc_pid_status"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "crash of a process with white space in its name"
        EXECUTABLE="white spaced name"
        rlRun "cp `which will_segfault` \"$EXECUTABLE\""

        prepare
        rlRun "./\"$EXECUTABLE\"" 139
        wait_for_hooks
        get_crash_path

        rlAssertEquals "Correct cmdline" "_'./$EXECUTABLE'" "_$(cat $crash_PATH/cmdline)"
        rlAssertEquals "Correct executable" "_$(pwd)/$EXECUTABLE" "_$(cat $crash_PATH/executable)"
        rlAssertEquals "Correct reason" "_$EXECUTABLE killed by SIGSEGV" "_$(cat $crash_PATH/reason)"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "ulimit -c $old_ulimit" 0
        rlBundleLogs abrt $(echo *_ls) $(echo verify_result*)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
