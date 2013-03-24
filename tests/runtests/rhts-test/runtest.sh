#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of rhts-test
#   Description: Verify reporter-rhtsupport functionality
#   Author: Denys Vlasenko <dvlasenk@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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
        TmpDir=$(mktemp -d)
        cp -R -- queries/*   "$TmpDir"
        cp -R -- problem_dir "$TmpDir"
        cp -- pyserve *.conf "$TmpDir"
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "reporter-rhtsupport --help &> null"
        rlRun "reporter-rhtsupport --help 2>&1 | grep 'Usage:'"
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create"
        ./pyserve \
                create_0hint \
                create_1create \
                dummy \
                dummy \
                dummy \
                &> server_create &
        sleep 1
        rlRun "reporter-rhtsupport -v -c rhtsupport.conf -d problem_dir/ &> client_create"
        kill %1

        #rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" client_create
        #rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt server* client*
        popd # TmpDir
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
