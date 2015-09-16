#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ureport-machineid
#   Description: Verify that every dump dir on RHEL7 contains machineid.
#   Author: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="ureport-machineid"
PACKAGE="abrt"
ABRT_EXE=/usr/libexec/abrt-action-generate-machine-id


rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "abrt-action-generate-machine-id sanity"
        rlLog "The tool exists"
        rlAssertExists $ABRT_EXE

        rlLog "Provides the list with available generators"
        rlRun "$ABRT_EXE -l &> a-a-g-machine-id-list.log"
        rlAssertGrep "^systemd$" a-a-g-machine-id-list.log
        rlAssertGrep "^sosreport_uploader-dmidecode$" a-a-g-machine-id-list.log
        rlAssertEquals "Supports exactly two generators" "_$(cat a-a-g-machine-id-list.log | wc -l)" "_2"

        rlLog "Correctly returns contents of /etc/machine-id"
        rlRun "$ABRT_EXE -g systemd -n &> a-a-g-machine-id-systemd.log"
        rlAssertEquals "No error messages" "_$(cat a-a-g-machine-id-systemd.log | wc -l)" "_1"
        rlAssertNotDiffer /etc/machine-id a-a-g-machine-id-systemd.log

        rlRun "$ABRT_EXE -g systemd -n -o a-a-g-machine-id-systemd1.log"
        rlAssertExists a-a-g-machine-id-systemd1.log

        # abrt-action-generate-machine-id does not write a new line on the last
        # line if "-o OUTPUT" is passed in its command line arguments
        rlRun "echo >> a-a-g-machine-id-systemd1.log"

        rlAssertEquals "No error messages" "_$(cat a-a-g-machine-id-systemd1.log | wc -l)" "_1"
        rlAssertNotDiffer /etc/machine-id a-a-g-machine-id-systemd1.log

        rlLog "Uses generator prefix for systemd generator"
        rlRun "$ABRT_EXE -g systemd &> a-a-g-machine-id-systemd2.log"
        rlAssertEquals "No error messages" "_$(cat a-a-g-machine-id-systemd2.log | wc -l)" "_1"
        rlAssertEquals "Use systemd= prefix" "_systemd=$(cat /etc/machine-id)" "_$(cat a-a-g-machine-id-systemd2.log)"

        rlLog "Can run sosreport_uploader-dmidecode generator"
        rlRun "$ABRT_EXE -g sosreport_uploader-dmidecode &> a-a-g-machine-id-sosreport.log"
        rlAssertEquals "No error messages" "_$(cat a-a-g-machine-id-sosreport.log | wc -l)" "_1"
        rlAssertGrep "sosreport_uploader-dmidecode=" a-a-g-machine-id-sosreport.log
    rlPhaseEnd

     rlPhaseStartTest "Every dump dir on RHEL7 must contain machineid"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlAssertExists "$crash_PATH/machineid"

        rlRun "$ABRT_EXE -o machine_id.log 2>machine_id_errors.log" 0 "run abrt-action-generate-machine-id"

        rlAssertGrep "systemd=$(cat /etc/machine-id)" $crash_PATH/machineid

        if which dmidecode; then
            rlAssertGrep "sosreport_uploader-dmidecode=" $crash_PATH/machineid
        else
            rlLog "dmidecode-less system"
        fi

        rlAssertEquals "The error log is empty" "_" "_$(cat machine_id_errors.log)"
        rlAssertNotDiffer "$crash_PATH/machineid" machine_id.log

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs $TEST $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
rlJournalEnd
