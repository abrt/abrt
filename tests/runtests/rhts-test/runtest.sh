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
        check_prior_crashes

        LANG=""
        export LANG

        TmpDir=$(mktemp -d)
        cp -R -- queries/*   "$TmpDir"
        cp -R -- problem_dir "$TmpDir"
        cp -- pyserve *.conf "$TmpDir"
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "reporter-rhtsupport --help >/dev/null 2>&1"
        rlRun "reporter-rhtsupport --help 2>&1 | grep 'Usage:'"
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create"
        ./pyserve \
                create_0hint \
                create_1create \
                create_2attach \
                >server_create 2>&1 &
        sleep 1
        # just in case. otherwise reporter asks "This problem was already reported, want to do it again?"
        rm problem_dir/reported_to 2>/dev/null
        rlRun "reporter-rhtsupport -v -c rhtsupport.conf -d problem_dir/ >client_create 2>&1"
        kill %1
        echo client_create:
        cat -n client_create

        rlAssertGrep "URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" client_create
        rlAssertGrep "RHTSupport:.* URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" problem_dir/reported_to
        rm -f problem_dir/reported_to
    rlPhaseEnd

    # testing -tCASE_NO
    rlPhaseStartTest "rhtsupport attach"
        ./pyserve \
                create_0hint \
                create_2attach \
                >server_create 2>&1 &
        sleep 1
        # just in case. otherwise reporter asks "This problem was already reported, want to do it again?"
        rm problem_dir/reported_to 2>/dev/null
        rlRun "reporter-rhtsupport -v -c rhtsupport.conf -d problem_dir/ -t00809787 >client_create 2>&1"
        kill %1
        echo client_create:
        cat -n client_create

        #-tCASE_NO does not do this:
        #rlAssertGrep "URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" client_create
        #rlAssertGrep "RHTSupport:.* URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" problem_dir/reported_to
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create with option -u"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "augtool rm /files/etc/libreport/plugins/ureport.conf/ContactEmail" 0 "remove ContactEmail settings from ureport.conf"
        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/URL http://127.0.0.1:12345/faf" 0 "set URL to ureport.conf"

        ./pyserve \
                ureport_submit \
                create_0hint \
                create_1create \
                ureport_attach \
                create_2attach \
                >server_create 2>&1 &
        sleep 1

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create"

        kill %1

        rlAssertGrep "Sending ABRT crash statistics data" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" client_create

        rlAssertGrep "Checking for hints" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/problems" client_create

        rlAssertGrep "Creating a new case" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases" client_create

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" client_create

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create -E
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*/" client_create

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

   rlPhaseStartTest "rhtsupport create with option -u with attach email"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/ContactEmail abrt@email.com" 0 "set ContactEmail settings to ureport.conf"

        ./pyserve \
                ureport_submit \
                create_0hint \
                create_1create \
                ureport_attach \
                ureport_attach \
                create_2attach \
                >server_create 2>&1 &
        sleep 1

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create"

        kill %1

        rlAssertGrep "Sending ABRT crash statistics data" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" client_create

        rlAssertGrep "Checking for hints" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/problems" client_create

        rlAssertGrep "Creating a new case" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases" client_create

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" client_create

        rlAssertGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create -E
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*/" client_create

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create with option -u (uReport has been already submitted, email is configured)"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "echo \"uReport: BTHASH=691cf824e3e07457156125636e86c50279e29496\" > $crash_PATH/reported_to" 0 "Add BTHASH to reported_to"

        ./pyserve \
                create_0hint \
                create_1create \
                ureport_attach \
                ureport_attach \
                create_2attach \
                >server_create 2>&1 &
        sleep 1

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create"

        kill %1

        rlAssertNOTGrep "Sending ABRT crash statistics data" client_create
        rlAssertNOTGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" client_create

        rlAssertGrep "uReport has already been submitted." client_create

        rlAssertGrep "Checking for hints" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/problems" client_create

        rlAssertGrep "Creating a new case" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases" client_create

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" client_create

        rlAssertGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create -E
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*/" client_create

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create with option -u (uReport has been already submitted, email is not configured)"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "augtool rm /files/etc/libreport/plugins/ureport.conf/ContactEmail" 0 "remove ContactEmail settings from ureport.conf"

        rlRun "echo \"uReport: BTHASH=691cf824e3e07457156125636e86c50279e29496\" > $crash_PATH/reported_to" 0 "Add BTHASH to reported_to"

        ./pyserve \
                create_0hint \
                create_1create \
                ureport_attach \
                create_2attach \
                >server_create 2>&1 &
        sleep 1

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create"

        kill %1

        rlAssertNOTGrep "Sending ABRT crash statistics data" client_create
        rlAssertNOTGrep "Connecting to http://127.0.0.1:12345/faf/reports/new/" client_create

        rlAssertGrep "uReport has already been submitted." client_create

        rlAssertGrep "Checking for hints" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/problems" client_create

        rlAssertGrep "Creating a new case" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases" client_create

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create
        rlAssertGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create
        rlAssertGrep "Connecting to http://127.0.0.1:12345/faf/reports/attach/" client_create

        rlAssertNOTGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create
        rlAssertNOTGrep "post('http://127.0.0.1:12345/faf/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create -E
        rlAssertGrep "Connecting to http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*/" client_create

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt server* client*
        popd # TmpDir
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
