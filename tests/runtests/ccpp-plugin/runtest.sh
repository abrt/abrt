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
        cp verify_core_backtrace.py verify_core_backtrace_length.py $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin works"
        generate_crash
        wait_for_hooks
        get_crash_path

        ARCHITECTURE=$(rlGetPrimaryArch)

        ls $crash_PATH > crash_dir_ls

        rlAssertExists "$crash_PATH/uuid"

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertGrep "/bin/will_segfault" "$crash_PATH/core_backtrace"
    rlPhaseEnd

    rlPhaseStartTest "core_backtrace contents"
        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $ARCHITECTURE 2>&1 > verify_result" 0
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "core_backtrace for stack overflow"
        prepare
        generate_stack_overflow_crash
        wait_for_hooks
        get_crash_path

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
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlRun "ulimit -c $old_ulimit" 0
        rlBundleLogs abrt $(echo *_ls) $(echo verify_result*)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
