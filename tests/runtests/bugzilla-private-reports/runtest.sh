#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   PURPOSE of bugzilla-private-reports
#   Description: Verify Bugzilla private reports functionality
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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

TEST="bugzilla-private-reports"
PACKAGE="abrt"
EVENT_REPORT_BUGZILLA_CONF="/etc/libreport/events/report_Bugzilla.conf"

function test_private_simple
{
    LOG_PREFIX=private_simple_"${LOG_PREFIX}"
    ./pyserve $LOG_PREFIX \
            User.login.response \
            Bug.search.response_no_duplicate \
            Bug.create.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            User.logout.response \
            | tee ${LOG_PREFIX}_server.log &
    sleep 2
    rlRun "$1 &>${LOG_PREFIX}_client.log"
    kill %1

    rlAssertGrep "groups:[[:space:]]*abrt_test_group" ${LOG_PREFIX}_Bug.create_call.log
    rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" ${LOG_PREFIX}_client.log
    rm -f problem_dir/reported_to
}

function test_cross_version_duplicate_bug
{
    LOG_PREFIX=private_cross_version_duplicate_"${LOG_PREFIX}"
    ./pyserve $LOG_PREFIX \
            User.login.response \
            Bug.search.response_duplicate \
            Bug.get.response \
            Bug.comments.response \
            Bug.create.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.update.response \
            User.logout.response \
            | tee ${LOG_PREFIX}_server.log &
    sleep 2
    rlRun "./expect yes $1 &>${LOG_PREFIX}_client.log"
    kill %1

    rlAssertGrep "groups:[[:space:]]*abrt_test_group" ${LOG_PREFIX}_Bug.create_call.log
    rlAssertGrep "dupe_of:[[:space:]]*851210" ${LOG_PREFIX}_Bug.update_call.log
    rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" ${LOG_PREFIX}_client.log
    rm -f problem_dir/reported_to
}

function test_private_duplicate
{
    LOG_PREFIX=private_duplicate_"${LOG_PREFIX}"
    ./pyserve ${LOG_PREFIX} \
            User.login.response \
            Bug.search.response_duplicate \
            Bug.get.response \
            Bug.comments.response \
            Bug.create.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.add_attachment.response \
            Bug.update.response \
            User.logout.response \
            | tee ${LOG_PREFIX}_server.log &
    sleep 2
    rlRun "./expect yes $1 &>${LOG_PREFIX}_client.log"
    kill %1

    rlAssertGrep "groups:[[:space:]]*abrt_test_group" ${LOG_PREFIX}_Bug.create_call.log
    rlAssertGrep "dupe_of:[[:space:]]*851210" ${LOG_PREFIX}_Bug.update_call.log
    rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" ${LOG_PREFIX}_client.log
    rm -f problem_dir/reported_to
}

function test_private_duplicate_declined
{
    LOG_PREFIX=private_duplicate_declined_"${LOG_PREFIX}"
    ./pyserve ${LOG_PREFIX} \
            User.login.response \
            Bug.search.response_duplicate \
            Bug.get.response \
            Bug.comments.response \
            User.logout.response \
            | tee ${LOG_PREFIX}_server.log &
    sleep 2
    rlRun "./expect no $1 &>${LOG_PREFIX}_client.log"
    kill %1

    rlAssertNotGrep "Bug.create" ${LOG_PREFIX}_server.log
    rlAssertNotExists problem_dir/reported_to
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp expect $TmpDir
        cp -R queries/* $TmpDir
        cp -R problem_dir $TmpDir
        cp pyserve bugzilla.conf $TmpDir

        rlFileBackup $EVENT_REPORT_BUGZILLA_CONF
        cp report_Bugzilla.conf $EVENT_REPORT_BUGZILLA_CONF

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "bugzilla private bug"
        LOG_PREFIX=cmdline test_private_simple "reporter-bugzilla -v -c bugzilla.conf -d problem_dir -g abrt_test_group problem_dir"
    rlPhaseEnd

    rlPhaseStartTest "bugzilla cross-version duplicate bug"
        LOG_PREFIX=cmdline test_cross_version_duplicate_bug "reporter-bugzilla -vvv -c bugzilla.conf -d problem_dir -g abrt_test_group problem_dir"
    rlPhaseEnd

    rlPhaseStartTest "bugzilla private, duplicate bug"
        LOG_PREFIX=cmdline test_private_duplicate "reporter-bugzilla -vvv -c bugzilla.conf -d problem_dir -g abrt_test_group problem_dir"
    rlPhaseEnd

    rlPhaseStartTest "bugzilla private, duplicate bug - declined"
        LOG_PREFIX=cmdline test_private_duplicate_declined "reporter-bugzilla -vvv -c bugzilla.conf -d problem_dir -g abrt_test_group problem_dir"
    rlPhaseEnd

#rlPhaseStartTest "event - bugzilla private bug"
#EDITOR=cat LOG_PREFIX=event test_private_simple "report-cli -v -e report_Bugzilla -- problem_dir"
#rlPhaseEnd
#
#rlPhaseStartTest "event - bugzilla cross-version duplicate bug"
#EDITOR=cat LOG_PREFIX=event test_cross_version_duplicate_bug "report-cli -v -e report_Bugzilla -- problem_dir"
#rlPhaseEnd
#
#rlPhaseStartTest "event - bugzilla private, duplicate bug"
#EDITOR=cat LOG_PREFIX=event test_private_duplicate "report-cli -v -e report_Bugzilla -- problem_dir"
#rlPhaseEnd
#
#rlPhaseStartTest "event - bugzilla private, duplicate bug - declined"
#EDITOR=cat LOG_PREFIX=event test_private_duplicate_declined "report-cli -v -e report_Bugzilla -- problem_dir"
#rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
        rlFileRestore
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
