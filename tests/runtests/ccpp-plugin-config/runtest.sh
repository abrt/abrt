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

TEST="ccpp-plugin-config"
PACKAGE="abrt"

CFG_FILE="/etc/abrt/plugins/CCpp.conf"
EVENT_FILE="/etc/libreport/events.d/ccpp_event.conf"

rlJournalStart
    rlPhaseStartSetup
        rlAssert0 "No prior crashes recorded" $(abrt-cli list | wc -l)
        if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
            rlDie "Won't proceed"
        fi

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlFileBackup $CFG_FILE
    rlPhaseEnd

    rlPhaseStartTest "Disable ccpp"
        rlFileBackup $EVENT_FILE

        rlRun "rm -f $EVENT_FILE" 0 "Remove $EVENT_FILE"
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssert0 "No crash recorded" $(abrt-cli list | wc -l)

        rlFileRestore # EVENT_FILE
    rlPhaseEnd
    rlPhaseStartTest "MakeCompatCore"
        rm -rf core.*
        rlLogInfo "MakeCompatCore = yes"
        rlRun "echo 'MakeCompatCore = yes' > $CFG_FILE" 0 "Set MakeCompatCore = yes"
        rlLog "Sleep for 30 seconds"
        sleep 30

        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlFileRestore # CFG_FILE
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        ls core* > make_compat_core_yes_pwd_ls
        core_fname="$(echo core*)"
        rlLog "$core_fname"
        rlAssertExists "$core_fname"
        rlRun "file $core_fname > file_output" 0 "Run file on coredump"
        cat file_output
        rlAssertGrep "core file" file_output
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rlRun "rm -f $core_fname" 0 "Remove local coredump"
        unset core_fname

        rlLogInfo "MakeCompatCore = no"
        rlRun "echo 'MakeCompatCore = no' > $CFG_FILE" 0 "Set MakeCompatCore = no"
        rlLog "Sleeping for 30 seconds"
        sleep 30

        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlFileRestore # CFG_FILE
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        ls core* > make_compat_core_no_pwd_ls
        core_fname="$(echo core*)"
        rlAssertNotExists "$core_fname"
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

        rlAssertDiffer make_compat_core_yes_pwd_ls make_compat_core_no_pwd_ls
        diff make_compat_core_yes_pwd_ls make_compat_core_no_pwd_ls > make_compat_core_diff
    rlPhaseEnd

    rlPhaseStartTest "SaveBinaryImage"
        rlLogInfo "SaveBinaryImage = yes"
        rlRun "echo 'SaveBinaryImage = yes' > $CFG_FILE" 0 "Set SaveBinaryImage = yes"
        rlLog "Sleep for 30 seconds"
        sleep 30

        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlFileRestore # CFG_FILE
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        ls $crash_PATH > save_binary_image_yes_ls
        rlAssertExists "$crash_PATH/binary"
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

        rlLogInfo "SaveBinaryImage = no"
        rlRun "echo 'SaveBinaryImage = no' > $CFG_FILE" 0 "Set SaveBinaryImage = no"
        rlLog "Sleeping for 30 seconds"
        sleep 30

        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
        crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash_PATH" ]; then
            rlFileRestore # CFG_FILE
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        ls $crash_PATH > save_binary_image_no_ls
        rlAssertNotExists "$crash_PATH/binary"
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

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
