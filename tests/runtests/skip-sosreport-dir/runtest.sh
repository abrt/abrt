#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of skip-sosreport-dir
#   Description: Test skipping of dirs with sosreport running
#   Author: Martin Kutlak <mkutlak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2019 Red Hat, Inc. All rights reserved.
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

TEST="skip-sosreport-test"
PACKAGE="abrt"
SOSREPORT_EVENT="/etc/libreport/events.d/sosreport_event.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        TmpDir=$(mktemp -d)
        pushd "$TmpDir" || exit

        # generate large file to fill problem directories space
        dd if=/dev/urandom of=hugh_mongous count=13337 bs=1024

        if [[ -f "$SOSREPORT_EVENT" ]]; then
            rlFileBackup --namespace sos "$SOSREPORT_EVENT"
            rm -f "$SOSREPORT_EVENT"
        fi
    rlPhaseEnd

    rlPhaseStartTest "[1/3] delete dir"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "cp $TmpDir/hugh_mongous $crash_PATH" 0 "Fill problem directory with a huge file"

        rlRun "abrt-action-trim-files -vvv -d 1M:$ABRT_CONF_DUMP_LOCATION &> $TmpDir/trim-files-output" 0
        rlRun "cat $TmpDir/trim-files-output"

        rlAssertNotExists "$crash_PATH"
        rlAssertGrep "deleting '$(basename $crash_PATH)'" "$TmpDir/trim-files-output"

        rlRun "abrt-cli rm $crash_PATH" 1 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "[2/3] skip dir with sosreport, delete it if without sosreport"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "cp $TmpDir/hugh_mongous $crash_PATH" 0 "Fill problem directory with a huge file"
        rlRun "touch $crash_PATH/sosreport.log" 0 "Create FAKE sosreport.log file"

        rlRun "abrt-action-trim-files -vvv -d 1M:$ABRT_CONF_DUMP_LOCATION &> $TmpDir/trim-files-skip-output" 0
        rlRun "cat $TmpDir/trim-files-skip-output"

        rlAssertExists "$crash_PATH"
        rlAssertGrep "$crash_PATH': sosreport is being generated." "$TmpDir/trim-files-skip-output"

        rlRun "rm $crash_PATH/sosreport.log" 0 "Remove FAKE sosreport.log file"

        rlRun "abrt-action-trim-files -vvv -d 1M:$ABRT_CONF_DUMP_LOCATION &> $TmpDir/trim-files-delete-output" 0
        rlRun "cat $TmpDir/trim-files-delete-output"

        rlAssertNotExists "$crash_PATH"
        rlAssertGrep "deleting '$(basename $crash_PATH)'" "$TmpDir/trim-files-delete-output"

        rlRun "abrt-cli rm $crash_PATH" 1 "Remove crash directory"
    rlPhaseEnd


    rlPhaseStartTest "[3/3] skip dir with sosreport, delete others"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "cp $TmpDir/hugh_mongous $crash_PATH" 0 "Fill problem directory with a huge file"
        rlRun "touch $crash_PATH/sosreport.log" 0 "Create FAKE sosreport.log file"

        first_crash_PATH="$crash_PATH"

        prepare
        generate_stack_overflow_crash
        wait_for_hooks
        get_crash_path

        rlRun "cp $TmpDir/hugh_mongous $crash_PATH" 0 "Fill problem directory with a huge file"

        second_crash_PATH="$crash_PATH"

        rlRun "abrt-action-trim-files -vvv -d 1M:$ABRT_CONF_DUMP_LOCATION &> $TmpDir/trim-files-skip-2-output" 0
        rlRun "cat $TmpDir/trim-files-skip-2-output"

        rlAssertExists "$first_crash_PATH"
        rlAssertGrep "$first_crash_PATH': sosreport is being generated." "$TmpDir/trim-files-skip-2-output"

        rlAssertNotExists "$second_crash_PATH"
        rlAssertGrep "deleting '$(basename $second_crash_PATH)'" "$TmpDir/trim-files-skip-2-output"

        rlRun "abrt-cli rm $first_crash_PATH" 0 "Remove crash directory"
        rlRun "abrt-cli rm $second_crash_PATH" 1 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
        if [[ -f "$SOSREPORT_EVENT" ]]; then
            rlFileRestore --namespace sos
        fi
        popd # TmpDir
        rm -rf "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
