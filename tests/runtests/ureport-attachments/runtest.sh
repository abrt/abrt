#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ureport-attachments
#   Description: Verify that uReport client correctly attaches information to a report on server
#   Author: Jakub Filak <jfilak@redhat.com>
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
. ../aux/lib.sh

TEST="ureport-attachments"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
    rlPhaseEnd

    rlPhaseStartTest
        ./pyserve &
        sleep 1

        uReport_URL="http://localhost:12345"
        export uReport_URL

        rlRun "reporter-ureport -A -B -d ex_crash_dir"
        rlRun "reporter-ureport -A -b 12345 -d ex_crash_dir"

        rlRun "reporter-ureport -a DEADBEAF -B -d ex_crash_dir"
        rlRun "reporter-ureport -a DEADBEAF -b 12345 -d ex_crash_dir"

        uReport_ContactEmail="environment@redhta.com"
        export uReport_ContactEmail

        rlRun "reporter-ureport -A -e argument@redhat.com -d ex_crash_dir"
        rlRun "reporter-ureport -A -E -d ex_crash_dir"

        rlRun "reporter-ureport -a DEADBEAF -e argument@redhat.com -d ex_crash_dir"
        rlRun "reporter-ureport -a DEADBEAF -E -d ex_crash_dir"

        uReport_UserComment="command line user comment"
        export uReport_UserComment

        rlRun "reporter-ureport -A -o \"$uReport_UserComment\" -d ex_crash_dir"
        rlRun "reporter-ureport -A -O -d ex_crash_dir"

        rlRun "reporter-ureport -a DEADBEAF -o \"$uReport_UserComment\" -d ex_crash_dir"
        rlRun "reporter-ureport -a DEADBEAF -O -d ex_crash_dir"

        rlRun "reporter-ureport -A -l \"http://bugzilla.redhat.com/1000000\" -T \"url1\" -d ex_crash_dir"
        rlRun "reporter-ureport -a DEADBEAF -l \"http://bugzilla.redhat.com/1000001\" -T \"url2\""

        rlRun "reporter-ureport -A -L \"URL\" -r \"Bugzilla\" -T \"url3\" -d ex_crash_dir"
        rlRun "reporter-ureport -a DEADBEAF -L \"URL\" -r \"Bugzilla\" -T \"url4\" -d ex_crash_dir"

        kill %1
    rlPhaseEnd

    rlPhaseStartTest "Dealing with invalid arguments"

        rlRun "reporter-ureport -A -l \"http://bugzilla.redhat.com/1000000\" -d ex_crash_dir" 1
        rlRun "reporter-ureport -A -L -l \"http://bugzilla.redhat.com/1000000\" -T \"url\" -d ex_crash_dir" 1
        rlRun "reporter-ureport -A -L \"msg\" -d ex_crash_dir" 1
        rlRun "reporter-ureport -A -L \"URL\" -T \"url\" -d ex_crash_dir" 1
        rlRun "reporter-ureport -A -L \"URL\" -r \"Bugzilla\" -d ex_crash_dir" 1

    rlPhaseEnd

    rlPhaseStartCleanup
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
