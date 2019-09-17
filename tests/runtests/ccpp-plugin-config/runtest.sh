#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-config
#   Description: Tests ccpp-plugin configuration
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

TEST="ccpp-plugin-config"
PACKAGE="abrt"

CFG_FILE="/etc/abrt/plugins/CCpp.conf"
EVENT_FILE="/etc/libreport/events.d/ccpp_event.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlFileBackup $CFG_FILE
    rlPhaseEnd

    rlPhaseStartTest "Disable ccpp"
        rlRun "/usr/sbin/abrt-install-ccpp-hook uninstall" 0 "Uninstall hook"
        rlRun "/usr/sbin/abrt-install-ccpp-hook is-installed" 1 "Is hook uninstalled"
        rlAssertGrep "core" /proc/sys/kernel/core_pattern
        rlAssertNotGrep "abrt" /proc/sys/kernel/core_pattern

        generate_crash

        rlAssert0 "No crash recorded" $(abrt status --bare)
        rlRun "/usr/sbin/abrt-install-ccpp-hook install" 0 "Restore hook"
    rlPhaseEnd
    rlPhaseStartTest "MakeCompatCore"
        rm -rf core.*

        rlRun "augtool set /files${CFG_FILE}/MakeCompatCore yes" 0 "Enabling MakeCompatCore"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ls core* > make_compat_core_yes_pwd_ls
        core_fname="$(echo core*)"
        rlLog "$core_fname"
        rlAssertExists "$core_fname"
        rlRun "file $core_fname > file_output" 0 "Run file on coredump"
        cat file_output
        rlAssertGrep "core file" file_output
        remove_problem_directory
        rlRun "rm -f $core_fname" 0 "Remove local coredump"
        unset core_fname

        rlRun "augtool set /files${CFG_FILE}/MakeCompatCore no" 0 "Disabling MakeCompatCore"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ls core* > make_compat_core_no_pwd_ls
        core_fname="$(echo core*)"
        rlAssertNotExists "$core_fname"
        remove_problem_directory

        rlAssertDiffer make_compat_core_yes_pwd_ls make_compat_core_no_pwd_ls
        diff make_compat_core_yes_pwd_ls make_compat_core_no_pwd_ls > make_compat_core_diff
    rlPhaseEnd

    rlPhaseStartTest "SaveBinaryImage"
        rlRun "augtool set /files${CFG_FILE}/SaveBinaryImage yes" 0 "Enabling SaveBinaryImage"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ls $crash_PATH > save_binary_image_yes_ls
        rlAssertExists "$crash_PATH/binary"
        remove_problem_directory

        rlRun "augtool set /files${CFG_FILE}/SaveBinaryImage no" 0 "Disabling SaveBinaryImage"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ls $crash_PATH > save_binary_image_no_ls
        rlAssertNotExists "$crash_PATH/binary"
        remove_problem_directory

        rlAssertDiffer save_binary_image_yes_ls save_binary_image_no_ls
        diff save_binary_image_yes_ls save_binary_image_no_ls > save_binary_image_diff
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore # CFG_FILE

        rlRun "ulimit -c $old_ulimit" 0

        rlBundleLogs abrt $(echo *_{ls,diff})
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
