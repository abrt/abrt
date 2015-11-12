#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-selinux
#   Description: Verify that abrt-hook-ccpp plays nice with selinux
#   Author: Jakub Filak <jfilak@redhat.com>
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

TEST="ccpp-plugin-selinux"
PACKAGE="abrt"
CFG_FILE="/etc/abrt/abrt-action-save-package-data.conf"
CCPP_CFG_FILE="/etc/abrt/plugins/CCpp.conf"
TEST_USER="abrt-selinux-test"
SELINUX_POLICY=abrt_ccpp_test_policy
SOURCE=segfault.c
EXECUTABLE=${SOURCE%.c}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        useradd $TEST_USER -M || rlDie "Cannot proceed without the user"
        echo "kokotice" | passwd $TEST_USER --stdin || rlDie "Failed to update password"

        SelinuxPolicyBuildDir=$(mktemp -d)
        cp $SELINUX_POLICY.te $SelinuxPolicyBuildDir || rlDie "Missing SELinux policy definition"
        make -C $SelinuxPolicyBuildDir -f /usr/share/selinux/devel/Makefile || rlDie "Failed to build SELinux policy"
        semodule -i $SelinuxPolicyBuildDir/$SELINUX_POLICY.pp || rlDie "Failed to load SELinux policy"

        rlFileBackup $CFG_FILE $CCPP_CFG_FILE

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        chown $TEST_USER $TmpDir
        chmod u+rwx $TmpDir
        chcon -t abrt_test_dir_t $TmpDir
        cp $SOURCE $TmpDir
        pushd $TmpDir

        sed -i 's/\(ProcessUnpackaged\) = no/\1 = yes/g' $CFG_FILE

        # Test sigchld AVC
        sed -i 's/\(CreateCoreBacktrace\) = no/\1 = yes/g' $CCPP_CFG_FILE
        sed -i 's/\(SaveFullCore\) = yes/\1 = no/g' $CCPP_CFG_FILE
    rlPhaseEnd

    rlPhaseStartTest
        rlLog "Build crasher"
        rlRun "gcc --std=c99 -Wall -Wextra -pedantic -o $EXECUTABLE $SOURCE"
        rlRun "chcon -t abrt_test_bin_exec_t ./$EXECUTABLE"

        rlLog "Generate crash"
        prepare
        rlRun "su $TEST_USER -c ./$EXECUTABLE" 139
        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -lZ`"
        CORE_NAME="core.$(cat $crash_PATH/global_pid)"
        rlAssertExists $CORE_NAME
        rlAssertEquals "Correct label" "_unconfined_u:object_r:abrt_test_file_t:s0" "_$(stat --format %C $CORE_NAME)"

        # Test sigchld AVC
        rlAssertExists "$crash_PATH/core_backtrace"

        rlRun "abrt-cli rm $crash_PATH"
        rlRun "rm $CORE_NAME"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore # CFG_FILE CCPP_CFG_FILE

        rlRun "semodule -r $SELINUX_POLICY"
        rlRun "ulimit -c $old_ulimit" 0
        rlRun "userdel -r -f $TEST_USER" 0

        rm -rf $SelinuxPolicyBuildDir

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
