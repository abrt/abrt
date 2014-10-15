#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-abrt-action-ureport
#   Description: Verify abrt-abrt-action-ureport functionality
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

TEST="rhts-test"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        LANG=""
        export LANG

        TmpDir=$(mktemp -d)
        cp -- fakefaf.py  "$TmpDir"
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "attaching contact email if configured"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks


        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/ContactEmail abrt@email.com" 0 "set ContactEmail settings to ureport.conf"
        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/URL http://127.0.0.1:12345/faf" 0 "set URL to ureport.conf"
        rlRun "echo 0 > $crash_PATH/ureports_counter" 0 "set ureports_counter to 0"

        ./fakefaf.py &> server.log &
        sleep 1

        pushd $crash_PATH
        /usr/libexec/abrt-action-ureport -vvv &> $TmpDir/ureport.log
        popd #$crash_PATH

        kill %1

        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" ureport.log
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" ureport.log
        rlAssertGrep "Attaching ContactEmail: abrt@email.com" ureport.log
        lAssertGrep "{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }" ureport.log

        rlAssertGrep "127.0.0.1 - - \[.*\] \"POST /faf/reports/new/ HTTP/1.1\" 202 -" server.log
        rlAssertGrep "127.0.0.1 - - \[.*\] \"POST /faf/reports/attach/ HTTP/1.1\" 202 -" server.log

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "do not attach contact email if not configured"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks


        rlRun "augtool rm /files/etc/libreport/plugins/ureport.conf/ContactEmail" 0 "rm ContactEmail settings frim ureport.conf"
        rlRun "echo 0 > $crash_PATH/ureports_counter" 0 "set ureports_counter to 0"

        ./fakefaf.py &> server.log &
        sleep 1

        pushd $crash_PATH
        /usr/libexec/abrt-action-ureport -vvv &> $TmpDir/ureport.log
        popd # crash_PATH

        kill %1

        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" ureport.log

        rlAssertNotGrep "Attaching ContactEmail: abrt@email.com" ureport.log
        lAssertNotGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" ureport.log
        lAssertNotGrep "{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }" ureport.log

        rlAssertGrep "127.0.0.1 - - \[.*\] \"POST /faf/reports/new/ HTTP/1.1\" 202 -" server.log
        rlAssertNotGrep "127.0.0.1 - - \[.*\] \"POST /faf/reports/attach/ HTTP/1.1\" 202 -" server.log
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd


    rlPhaseStartCleanup
        rlBundleLogs abrt server* client*
        popd # TmpDir
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
