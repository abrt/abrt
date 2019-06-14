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
EVENT_FILE="/etc/libreport/events.d/ccpp_event.conf"

NEW_PID_NS_PRG=will_segfault_in_new_pid

UNPRIV_USER_NAME=abrt_ci_test_user
UNPRIV_TO_ROOT_PRG=unprivileged_to_root
ROOT_TO_UNPRIV_PRG=root_to_unprivileged

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d /var/tmp/abrt-test.XXXXXX)
        chmod o+rx $TmpDir

        rlRun "cp verify_core_backtrace.py $NEW_PID_NS_PRG.c $UNPRIV_TO_ROOT_PRG.c $ROOT_TO_UNPRIV_PRG.c $TmpDir/" || rlDie "Missing files"

        pushd $TmpDir

        rlRun "gcc -std=gnu99 -pedantic -Wall -Wextra -Wno-unused-parameter -o $NEW_PID_NS_PRG $NEW_PID_NS_PRG.c" || rlDie "Failed to build - $NEW_PID_NS_PRG"
        rlRun "gcc -std=c99 -pedantic -Wall -Wextra -o $ROOT_TO_UNPRIV_PRG $ROOT_TO_UNPRIV_PRG.c" || rlDie "Failed to build - $ROOT_TO_UNPRIV_PRG"
        rlRun "gcc -std=c99 -pedantic -Wall -Wextra -o $UNPRIV_TO_ROOT_PRG $UNPRIV_TO_ROOT_PRG.c" || rlDie "Failed to build - $UNPRIV_TO_ROOT_PRG"
        chown root:root $UNPRIV_TO_ROOT_PRG
        chmod u+s $UNPRIV_TO_ROOT_PRG

        rlFileBackup $CFG_FILE $EVENT_FILE

        rlLogInfo "Removing core_backtrace generator from ccpp_event.conf"
        rlRun "sed '/^.*core_backtrace.*$/d' -i $EVENT_FILE"

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        old_suid_dumpable=$(cat /proc/sys/fs/suid_dumpable)
        rlRun "echo 2 > /proc/sys/fs/suid_dumpable" 0

        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged yes"
    rlPhaseEnd

    rlPhaseStartTest "JITCoreDumpTracing enabled"
        rlLogInfo "VerboseLog = 3"
        rlLogInfo "JITCoreDumpTracing = yes"
        rlRun "echo 'VerboseLog = 3' > $CFG_FILE" 0 "Set VerboseLog = 3"
        rlRun "echo 'JITCoreDumpTracing = yes' >> $CFG_FILE" 0 "Set JITCoreDumpTracing = yes"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "JITCoreDumpTracing enabled - New PID namespace"
        # I did not use 'unshare --fork --pid will_segfault' because unshare
        # kills itself with the signal the child received.
        rlLogInfo "Build the binary"

        prepare
        rlRun "./$NEW_PID_NS_PRG"
        wait_for_hooks
        get_crash_path
        rlRun "killall abrt-hook-ccpp" 1 "Kill hung abrt-hook-ccpp process"

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertExists "$crash_PATH/coredump"
        rlAssertNotEquals "TID is the global TID" "_1" "_$(cat $crash_PATH/tid)"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "SaveFullCore disabled"
        rlLogInfo "VerboseLog = 3"
        rlLogInfo "JITCoreDumpTracing = yes"
        rlLogInfo "SaveFullCore = no"
        rlRun "echo 'VerboseLog = 3' > $CFG_FILE" 0 "Set VerboseLog = 3"
        rlRun "echo 'JITCoreDumpTracing = yes' >> $CFG_FILE" 0 "Set JITCoreDumpTracing = yes"
        rlRun "echo 'SaveFullCore = no' >> $CFG_FILE" 0 "Set SaveFullCore = no"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "All frames must have required members"

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

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "All frames must have required members"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "Unprivileged user running root set-uid program"
        prepare

        rlRun "useradd -M -d /tmp $UNPRIV_USER_NAME"
        rlRun "su $UNPRIV_USER_NAME -c \"./$UNPRIV_TO_ROOT_PRG\"" 139
        rlRun "userdel -f $UNPRIV_USER_NAME"

        wait_for_hooks
        get_crash_path

        rlAssertEquals "Ran under root" $(cat ${crash_PATH}/uid) 0

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "Validating generated core_backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "root running set-uid program as unprivileged user"

        prepare

        rlRun "useradd -M -d /tmp $UNPRIV_USER_NAME"
        UNPRIV_USER_ID=$(id -u $UNPRIV_USER_NAME)
        rlRun "./$ROOT_TO_UNPRIV_PRG $UNPRIV_USER_NAME" 139
        rlRun "userdel -f $UNPRIV_USER_NAME"

        wait_for_hooks
        get_crash_path

        rlAssertEquals "Ran under unpriv" $(cat ${crash_PATH}/uid) $UNPRIV_USER_ID

        rlAssertExists "$crash_PATH/core_backtrace"
        rlAssertNotExists "$crash_PATH/coredump"

        rlRun "./verify_core_backtrace.py $crash_PATH/core_backtrace $(uname -i) $(cat ${crash_PATH}/executable)" 0 "Validating generated core_backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore # CFG_FILE

        rlRun "ulimit -c $old_ulimit" 0
        rlRun "echo $old_suid_dumpable > /proc/sys/fs/suid_dumpable" 0
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged no"

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
