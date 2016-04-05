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

function new_dump_dir
{
    mkdir $1
    date +%s              > $1/time
    echo $TEST            > $1/type
    echo $TEST            > $1/analyzer
    echo 2                > $1/count
    echo "/bin/sleep"     > $1/executable
    rpm -qf `which sleep` > $1/package
    echo "Red Hat, Inc."  > $1/pkg_vendor
    echo "coreutils"      > $1/component
    echo "Red Hat Enterprise Linux Server release 6.8" > $1/os_release
}

function run_without_interruption
{
    rlRun "./expect ? reporter-rhtsupport -vvv -c rhtsupport_local.conf -d $1 &>$1.log"
    rlAssertNotGrep "GOT QUESTION:" $1.log
}

function run_with_question
{
    rlRun "./expect no reporter-rhtsupport -vvv -c rhtsupport_local.conf -d $2 &>$2_no.log"
    rlAssertGrep "GOT QUESTION: $1" $2_no.log
    rlAssertEquals "Exactly one question" "_1" "_$(grep -c 'GOT QUESTION:' $2_no.log)"

    rlRun "./expect yes reporter-rhtsupport -vvv -c rhtsupport_local.conf -d $2 &>$2_yes.log"
    rlAssertGrep "GOT QUESTION: $1" $2_yes.log
    rlAssertEquals "Exactly one question" "_1" "_$(grep -c 'GOT QUESTION:' $2_yes.log)"
}

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf
        check_prior_crashes

        LANG=""
        export LANG

        TmpDir=$(mktemp -d)
        cp -R -- queries/*   "$TmpDir"
        cp -R -- problem_dir "$TmpDir"
        cp -- pyserve *.conf expect "$TmpDir"
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "reporter-rhtsupport --help >/dev/null 2>&1"
        rlRun "reporter-rhtsupport --help 2>&1 | grep 'Usage:'"

        new_dump_dir reportable_dump_directory
        run_without_interruption reportable_dump_directory
    rlPhaseEnd

    rlPhaseStartTest "Vendor check"
        rlLog "No Vendor"
        new_dump_dir no_vendor
        rm -f no_vendor/pkg_vendor
        run_without_interruption no_vendor

        rlLog "Fedora Project"
        new_dump_dir fedora_vendor
        echo "Fedora Project" > fedora_vendor/pkg_vendor
        run_with_question vendor fedora_vendor
    rlPhaseEnd

    rlPhaseStartTest "Local occurence counter check"
        rlLog "No count"
        new_dump_dir no_count
        rm -f no_count/count

        rlLog "Not a number"
        new_dump_dir not_a_number
        echo "nan" > not_a_number/count
        run_without_interruption

        rlLog "Count == 0"
        new_dump_dir  count_zero
        echo 0 > count_zero/count
        run_without_interruption

        rlLog "Count == 99999999999999999"
        new_dump_dir count_huge
        echo 99999999999999999 > count_huge/count
        run_without_interruption

        rlLog "Count == 1"
        new_dump_dir  count_one
        echo 1 > count_one/count
        run_with_question count count_one

        rlLog "Count > 1 && unknown reproducible"
        new_dump_dir unknown_reproducible
        echo 1 > unknown_reproducible/count
        echo "Not sure how to reproduce the problem" > unknown_reproducible/reproducible
        run_with_question count unknown_reproducible

        rlLog "Count == 1 && reproducible"
        new_dump_dir reproducible
        echo 1 > reproducible/count
        echo "The problem is reproducible" > reproducible/reproducible
        run_without_interruption

        rlLog "Count == 1 && recurrent"
        new_dump_dir recurrent
        echo 1 > recurrent/count
        echo "The problem occurs regularly" > recurrent/reproducible
        run_without_interruption

        rlLog "Count == 1 && not supported reproducible"
        new_dump_dir not_supported_reproducible
        echo 1 > not_supported_reproducible/count
        echo "Random text" > not_supported_reproducible/reproducible
        run_with_question count not_supported_reproducible
    rlPhaseEnd

    rlPhaseStartTest "Packaged check"
        rlLog "No Package"
        new_dump_dir no_package
        rm -f no_package/package
        run_with_question package no_package
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs interaction $(ls *.log)
        rm -f *.log
    rlPhaseEnd

    rlPhaseStartSetup
#REPORT_CLIENT_NONINTERACTIVE=1
#        export REPORT_CLIENT_NONINTERACTIVE
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

        echo 2 > $crash_PATH/count
        echo "Red Hat, Inc." > $crash_PATH/pkg_vendor
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

        echo 2 > $crash_PATH/count
        echo "Red Hat, Inc." > $crash_PATH/pkg_vendor
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

        echo 2 > $crash_PATH/count
        echo "Red Hat, Inc." > $crash_PATH/pkg_vendor
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

        echo 2 > $crash_PATH/count
        echo "Red Hat, Inc." > $crash_PATH/pkg_vendor
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

        rlAssertNotGrep "Linking ABRT crash statistics record with contact email: 'abrt@email.com'" client_create6
        rlAssertNotGrep "post('http://127.0.0.1:12345/rs/telemetry/abrt/reports/attach/','{ \"bthash\": \"691cf824e3e07457156125636e86c50279e29496\", \"type\": \"email\", \"data\": \"abrt@email.com\" }')" client_create6

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
