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
        load_abrt_conf
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

        # pyserver stores received data as $RECEIVED_FILE
        RECEIVED_FILE="data.tar.gz"
        rlAssertExists $RECEIVED_FILE
        rlRun "tar -zxvf $RECEIVED_FILE"

        rlAssertExists "content.xml"
        rlAssertExists "content"

        # test wether the recieved data are the same as data in the problem dir
        rlRun "diff -r problem_dir content &> dir_diff.log"
        rlLog "diff dir content"
        cat "dir_diff.log"

        rm -rf content content.xml $RECEIVED_FILE
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
        rlRun "reporter-rhtsupport -v -c rhtsupport.conf -d problem_dir/ -t00809787 >client_create2 2>&1"
        kill %1
        echo client_create:
        cat -n client_create2

        #-tCASE_NO does not do this:
        #rlAssertGrep "URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" client_create2
        #rlAssertGrep "RHTSupport:.* URL=http://127.0.0.1:12345/rs/cases/[0-9]*/attachments/.*" problem_dir/reported_to
        rm -f problem_dir/reported_to
    rlPhaseEnd

    rlPhaseStartTest "rhtsupport create with option -u"
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "augtool rm /files/etc/libreport/plugins/ureport.conf/ContactEmail" 0 "remove ContactEmail settings from ureport.conf"

        ./pyserve \
                ureport_submit \
                create_0hint \
                create_1create \
                ureport_attach \
                create_2attach \
                >server_create 2>&1 &
        sleep 1

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create3"

        kill %1

        rlAssertGrep "Sending ABRT crash statistics data" client_create3
        rlAssertGrep "curl: Connected to 127.0.0.1 (127.0.0.1) port 12345 (#0)" client_create3
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/new/ HTTP/1" client_create3

        rlAssertGrep "Checking for hints" client_create3
        rlAssertGrep "curl sent header: 'POST /rs/problems HTTP/1" client_create3

        rlAssertGrep "Creating a new case" client_create3
        rlAssertGrep "curl sent header: 'POST /rs/cases HTTP/1" client_create3

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create3
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create3
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/attach/ HTTP/1" client_create3

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create3 -E
        rlAssertGrep "curl sent header: 'POST /rs/cases/[0-9]*/attachments/.*/comments HTTP/1" client_create3

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/last-ccpp"
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

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create4"

        kill %1

        rlAssertGrep "Sending ABRT crash statistics data" client_create4
        rlAssertGrep "curl: Connected to 127.0.0.1 (127.0.0.1) port 12345 (#0)" client_create4
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/new/ HTTP/1" client_create4

        rlAssertGrep "Checking for hints" client_create4
        rlAssertGrep "curl sent header: 'POST /rs/problems HTTP/1" client_create4

        rlAssertGrep "Creating a new case" client_create4
        rlAssertGrep "curl sent header: 'POST /rs/cases HTTP/1" client_create4

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create4
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create4
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/attach/ HTTP/1" client_create4

        rlAssertGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create4
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create4

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create4 -E
        rlAssertGrep "curl sent header: 'POST /rs/cases/[0-9]*/attachments/.*/comments HTTP/1" client_create4

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/last-ccpp"
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

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create5"

        kill %1

        # uReports counting does not work yet
        #rlAssertNotGrep "Sending ABRT crash statistics data" client_create5
        #rlAssertNotGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/new/ HTTP/1" client_create5

        rlAssertGrep "uReport has already been submitted." client_create5

        rlAssertGrep "Checking for hints" client_create5
        rlAssertGrep "curl sent header: 'POST /rs/problems HTTP/1" client_create5

        rlAssertGrep "Creating a new case" client_create5
        rlAssertGrep "curl sent header: 'POST /rs/cases HTTP/1" client_create5

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create5
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create5
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/attach/ HTTP/1" client_create5

        rlAssertGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create5
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create5

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create5 -E
        rlAssertGrep "curl sent header: 'POST /rs/cases/[0-9]*/attachments/.*/comments HTTP/1" client_create5

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION/last-ccpp"
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

        rlRun "reporter-rhtsupport -vvv -u -c rhtsupport.conf -d $crash_PATH &> client_create6"

        kill %1

        # uReports counting does not work yet
        #rlAssertNotGrep "Sending ABRT crash statistics data" client_create6
        #rlAssertNotGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/new/ HTTP/1" client_create6

        rlAssertGrep "uReport has already been submitted." client_create6

        rlAssertGrep "Checking for hints" client_create6
        rlAssertGrep "curl sent header: 'POST /rs/problems HTTP/1" client_create6

        rlAssertGrep "Creating a new case" client_create6
        rlAssertGrep "curl sent header: 'POST /rs/cases HTTP/1" client_create6

        rlAssertGrep "Linking ABRT crash statistics record with the case" client_create6
        rlAssertGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"RHCID\", \"data\": \"http:\\\/\\\/127.0.0.1:12345\\\/rs\\\/cases\\\/00809787\\\/attachments\\\/382c3498-0f19-3edc-aa56-580cf0bc7251\" }')" client_create6
        rlAssertGrep "curl sent header: 'POST /rs/telemetry/abrt/reports/attach/ HTTP/1" client_create6

        rlAssertNOTGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create6
        rlAssertNOTGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create6

        rlAssertGrep "(Attaching problem data to case)|(Adding comment to case) 'http://127.0.0.1:12345/rs/cases/00809787/attachments/382c3498-0f19-3edc-aa56-580cf0bc7251'" client_create6 -E
        rlAssertGrep "curl sent header: 'POST /rs/cases/[0-9]*/attachments/.*/comments HTTP/1" client_create6

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls server* client*)
        popd # TmpDir
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
