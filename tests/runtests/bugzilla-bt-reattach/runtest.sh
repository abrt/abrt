#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bugzilla-bt-reattach
#   Description: Verify bugzilla backtrace reattaching functionality
#   Author: Richard Marko <rmarko@redhat.com>
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

TEST="bugzilla-bt-reattach"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -R queries/* $TmpDir
        cp -R problem_dir $TmpDir
        cp pyserve bugzilla.conf $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "reporter-bugzilla --help &> null"
        rlRun "reporter-bugzilla --help 2>&1 | grep 'Usage:'"
    rlPhaseEnd

    rlPhaseStartTest "bugzilla create"
        ./pyserve \
                version_response \
                0* \
                1no_duplicates_response \
                2bug_created_response \
                dummy \
                dummy \
                dummy \
                &> server_create &
        sleep 1
        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir/ &> client_create"
        kill %1

        rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" client_create
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartTest "better backtrace"
        # bug details response has backtrace rating 3
        echo '4' > problem_dir/backtrace_rating
        rlLog "Local backtrace rating $( cat problem_dir/backtrace_rating )"
        ./pyserve \
                version_response \
                0* \
                1duplicates_response \
                1duplicates_response \
                2bug_details \
                dummy \
                dummy \
                dummy \
                dummy \
                &> server_better &
        sleep 1
        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir/ &> client_better"
        kill %1

        rlAssertGrep "Bug is already reported: 772488" client_better
        rlAssertGrep "Adding abrt@mailinator.com to CC list" client_better
        rlAssertGrep "Adding new comment to bug 772488" client_better
        rlAssertGrep "Attaching better backtrace" client_better
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartTest "worse backtrace"
        echo '1' > problem_dir/backtrace_rating
        rlLog "Local backtrace rating $( cat problem_dir/backtrace_rating )"

        ./pyserve \
                version_response \
                0* \
                1duplicates_response \
                1duplicates_response \
                2bug_details \
                dummy \
                dummy \
                dummy \
                dummy \
                &> server_worse &
        sleep 1
        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir/ &> client_worse"
        kill %1

        rlAssertGrep "Adding new comment to bug 772488" client_worse
        rlAssertNotGrep "Attaching better backtrace" client_worse
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartTest "equal backtrace"
        echo '3' > problem_dir/backtrace_rating
        rlLog "Local backtrace rating $( cat problem_dir/backtrace_rating )"

        ./pyserve \
                version_response \
                0* \
                1duplicates_response \
                1duplicates_response \
                2bug_details \
                dummy \
                dummy \
                dummy \
                dummy \
                &> server_equal &
        sleep 1
        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir/ &> client_equal"
        kill %1

        rlAssertGrep "Adding new comment to bug 772488" client_equal
        rlAssertNotGrep "Attaching better backtrace" client_equal
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt server* client*
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
