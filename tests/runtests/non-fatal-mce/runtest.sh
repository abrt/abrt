#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of non-fatal-mce
#   Description: test ability to detect non-fatal Machine Check Exceptions
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2014 Red Hat, Inc. All rights reserved.
#
#   This program is free software: you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of
#   the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE.  See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program. If not, see http://www.gnu.org/licenses/.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="non-fatal-mce"
PACKAGE="abrt"
MCE_REQUIRED_FILES="kernel uuid duphash dmesg not-reportable
pkg_name pkg_arch pkg_epoch pkg_release pkg_version"

MCE_LOG="/var/log/mcelog"
MESSAGE_LOG="/var/log/messages"
OOPS_NONFATAL="oops-non-fatal-mce.test"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
    rlPhaseEnd

    rlPhaseStartTest MCE
        prepare

        # See ../../../doc/MCE_readme.txt
        rlRun "modprobe mce-inject"
        rlRun "sync &"
        rlRun "sleep 1"
        rlRun "sync"
        rlRun "mce-inject mce.cfg"

        # This module cannot be removed
        #rlRun "rmmod mce-inject"

        # Do not be scared by timeouts, kernel sometimes fails to detect MCE.
        wait_for_hooks
        get_crash_path

        for f in $MCE_REQUIRED_FILES; do
            rlAssertExists "$crash_PATH/$f"
        done

        rlAssertGrep "kernel" "$crash_PATH/pkg_name"
        rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"
        rlAssertGrep "The kernel log indicates that hardware errors were detected." "$crash_PATH/backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest "MCE JOURNAL - Get mcelog from journal"
        prepare

        if [ -f "$MCE_LOG" ]; then
            rlFileBackup --namespace "mcelog" "$MCE_LOG"
            rlRun "rm -f $MCE_LOG" 0 "Removing $MCE_LOG"
        fi

        if [ -f "$MESSAGE_LOG" ]; then
            rlFileBackup --namespace "messages" "$MESSAGE_LOG"
            rlRun "rm -f $MESSAGE_LOG" 0 "Removing $MESSAGE_LOG"
        fi

        # See ../../../doc/MCE_readme.txt
        rlRun "mce-inject mce.cfg"

        # Do not be scared by timeouts, kernel sometimes fails to detect MCE.
        wait_for_hooks
        get_crash_path

        for f in $MCE_REQUIRED_FILES; do
            rlAssertExists "$crash_PATH/$f"
        done

        rlAssertGrep "kernel" "$crash_PATH/pkg_name"
        rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"
        rlAssertGrep "The kernel log indicates that hardware errors were detected." "$crash_PATH/backtrace"
        rlAssertGrep "The last 20 mcelog lines of system log are" "$crash_PATH/backtrace"
        rlRun "cat $crash_PATH/backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

        if [ -f "$MCE_LOG" ]; then
            rlFileRestore --namespace "mcelog"
        fi

        if [ -f "$MESSAGE_LOG" ]; then
            rlFileRestore --namespace "messages"
        fi
    rlPhaseEnd

    rlPhaseStartTest "MCE JOURNAL - journal is empty"
        prepare

        if [ -f "$MCE_LOG" ]; then
            rlFileBackup --namespace "mcelog" "$MCE_LOG"
            rlRun "rm -f $MCE_LOG" 0 "Removing $MCE_LOG"
        fi

        if [ -f "$MESSAGE_LOG" ]; then
            rlFileBackup --namespace "messages" "$MESSAGE_LOG"
            rlRun "rm -f $MESSAGE_LOG" 0 "Removing $MESSAGE_LOG"
        fi

        # backup journal logs before we delete them
        rlFileBackup --clean --namespace "journal" "/var/log/journal"
        rlRun "rm -rf /var/log/journal/*" 0 "!! Removing all journal logs !!"
        rlRun "EXAMPLES_PATH=\"../../examples\""
        rlRun "abrt-dump-oops -xD $EXAMPLES_PATH/$OOPS_NONFATAL 2>&1" 0 "Dump MCE OOPS"

        # Do not be scared by timeouts, kernel sometimes fails to detect MCE.
        wait_for_hooks
        get_crash_path

        for f in $MCE_REQUIRED_FILES; do
            rlAssertExists "$crash_PATH/$f"
        done

        rlAssertGrep "kernel" "$crash_PATH/pkg_name"
        rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"
        rlAssertGrep "The kernel log indicates that hardware errors were detected." "$crash_PATH/backtrace"
        rlAssertGrep "However, neither /var/log/mcelog nor system log contain mcelog messages" "$crash_PATH/backtrace"
        rlRun "cat $crash_PATH/backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

        if [ -f "$MCE_LOG" ]; then
            rlFileRestore --namespace "mcelog"
        fi

        if [ -f "$MESSAGE_LOG" ]; then
            rlFileRestore --namespace "messages"
        fi

        # restore deleted journal logs
        rlFileRestore --namespace "journal"
    rlPhaseEnd

    rlPhaseStartTest "rhbz1064458"
        # If there happens non-fatal MCE, it is impossible to report any
        # further oops as dmesg contains the "Machine check events logged" string which
        # leads abrt to mark all subsequent oopses as MCE and therefore irreportable.

        prepare

        rlRun "EXAMPLES_PATH=\"../../examples\""

        # Hopefully we don't remove running kernel's package
        rlRun "sed s/2.6.27.9-159.fc10.i686/$(uname -r)/ $EXAMPLES_PATH/oops1.test | abrt-dump-oops -xD 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "Found OOPS"

        wait_for_hooks
        get_crash_path

        rlAssertNotExists "$crash_PATH/not-reportable"
        rlAssertNotGrep "The kernel log indicates that hardware errors were detected." "$crash_PATH/backtrace"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
