#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of kernel-vmcore-harvest
#   Description: Tests moving kernel core dumps at startup
#   Author: Petr Kubat <pkubat@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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
. ../../../aux/lib.sh

TEST="kernel-vmcore-harvest"
PACKAGE="abrt"
REQUIRED_FILES="analyzer architecture component
last_occurrence os_info os_release time type uid uuid vmcore"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel
        load_abrt_conf
    rlPhaseEnd

    rlPhaseStartTest
        prepare

        rlRun "mkdir -p /var/crash/test" 0 "Creating vmcore dir"
        rlRun "echo testing > /var/crash/test/vmcore" 0 "Creating vmcore"
        rlLogInfo "Restarting abrtd"
        systemctl restart  abrtd.service

        wait_for_hooks

        check_dump_dir_attributes_vmcore_rhel "${ABRT_CONF_DUMP_LOCATION}/vmcore-test"

        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-test"
        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-test/analyzer"
        for f in $REQUIRED_FILES; do
                rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-test/$f"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf /var/crash/test" 0 "Removing vmcore from /var/crash/"
        rlRun "rm -rf ${ABRT_CONF_DUMP_LOCATION}/vmcore-test" 0 "Removing vmcore from the abrt dump location"
    rlPhaseEnd

    rlPhaseStartTest "kdump's vmcore-dmesg.txt"
        prepare

        TEST_ID="test-dmesg"
        rlRun "mkdir -p /var/crash/${TEST_ID}" 0 "Creating vmcore dir"
        rlRun "echo ${TEST_ID} > /var/crash/${TEST_ID}/vmcore" 0 "Creating vmcore"
        rlRun "cp -v vmcore-dmesg.txt /var/crash/${TEST_ID}/" 0 "Adding vmcore-dmesg.txt"

        rlLogInfo "Restarting abrtd"
        systemctl restart  abrtd.service

        wait_for_hooks

        check_dump_dir_attributes_vmcore_rhel "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}"

        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}"
        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}/analyzer"
        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}/backtrace"
        rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}/reason"
        rlAssertGrep "kernel paging request at source_load" "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}/reason"
        for f in $REQUIRED_FILES; do
                rlAssertExists "${ABRT_CONF_DUMP_LOCATION}/vmcore-${TEST_ID}/$f"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf /var/crash/test-dmesg" 0 "Removing vmcore from /var/crash/"
        rlRun "rm -rf ${ABRT_CONF_DUMP_LOCATION}/vmcore-test-dmesg" 0 "Removing vmcore from the abrt dump location"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
