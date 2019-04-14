#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-cli-report-mantisbt
#   Description: Verify abrt-cli-report-mantisbt functionality
#   Author: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="abrt-cli-report-mantisbt"
PACKAGE="abrt"

TEST_DIR="."
QUERIES_DIR="."

ureport_conf="/etc/libreport/events/report_uReport.conf"
retrace_conf="/etc/libreport/events/analyze_RetraceServer.conf"
mantis_format_conf="/etc/libreport/plugins/mantisbt_format_test.conf"
report_centos_conf="/etc/libreport/events/report_CentOSBugTracker.conf"
abrt_action_conf="/etc/abrt/abrt-action-save-package-data.conf"
retrace_event_conf="/etc/libreport/events.d/ccpp_retrace_event.conf"

faf_server_port=12345
retrace_server_port=12346
mantisbt_port=12347

function set_configuration()
{
    # backups
    rlRun "touch $ureport_conf" # is possible the file does not exists
    rlRun "cp -v $ureport_conf $ureport_conf'.backup'" 0

    rlRun "touch $retrace_conf" # is possible the file does not exists
    rlRun "cp -v $retrace_conf $retrace_conf'.backup'" 0

    rlRun "cp -v $report_centos_conf $report_centos_conf'.backup'" 0
    rlRun "cp -v $abrt_action_conf $abrt_action_conf'.backup'" 0
    rlRun "cp -v $retrace_event_conf $retrace_event_conf'.backup'" 0

    # ureport
    cat > $ureport_conf << EOF
uReport_URL = 127.0.0.1:$faf_server_port
uReport_WatchReportedBugs = no
uReport_ContactEmail =
uReport_SSLVerify = no
EOF

    rlAssert0 "set $ureport_conf" $?

    # retrace server
    cat > $retrace_conf << EOF
RETRACE_SERVER_URI = 127.0.0.1
RETRACE_SERVER_INSECURE = insecure
EOF

    rlAssert0 "set $retrace_conf" $?
    rlRun "export RETRACE_SERVER_PORT=$retrace_server_port" 0 "exporting RETRACE_SERVER_PORT env"

    cat > $report_centos_conf << EOF
Mantisbt_MantisbtURL = localhost:$mantisbt_port
Mantisbt_Login =
Mantisbt_Password =
Mantisbt_SSLVerify = no
EOF

    rlAssert0 "set $report_centos_conf" $?

    # format
    cat > $mantis_format_conf << EOF
%summary:: [abrt] %pkg_name%[[: %crash_function%()]][[: %reason%]][[: TAINTED %tainted_short%]]

Description of problem:: %bare_comment

Version-Release number of selected component:: %bare_package

Truncated backtrace:: %bare_%short_backtrace

%Additional info::
:: -pkg_arch,-pkg_epoch,-pkg_name,-pkg_release,-pkg_version,\
        >--->----component,-architecture,\
        >----analyzer,-count,-duphash,-uuid,-abrt_version,\
        >----username,-hostname,-os_release,-os_info,\
        >----time,-pid,-pwd,-last_occurrence,-ureports_counter,\
        >---%reporter,\
        >---%oneline

%attach:: backtrace, maps
EOF

    rlAssert0 "set $mantis_format_conf" $?

    cat > $abrt_action_conf << EOF
OpenGPGCheck = no
BlackList = nspluginwrapper, valgrind, strace, mono-core
ProcessUnpackaged = no
BlackListedPaths = /usr/share/doc/*, */example*, /usr/bin/nspluginviewer, /usr/lib/xulrunner-*/plugin-container
Interpreters = python2, python2.7, python, python3, python3.3, perl, perl5.16.2
EOF

    rlAssert0 "set $abrt_action_conf" $?

    cat > $retrace_event_conf << EOF
EVENT=analyze_RetraceServer type=CCpp
        abrt-retrace-client batch -k --headers --dir "\$DUMP_DIR" --status-delay 10 &&
        abrt-action-analyze-backtrace
EOF

    rlAssert0 "set $retrace_event_conf" $?
}

function restore_configuration()
{
    rlRun "cp -v $ureport_conf'.backup' $ureport_conf" 0
    rlRun "cp -v $retrace_conf'.backup' $retrace_conf" 0
    rlRun "cp -v $report_centos_conf'.backup' $report_centos_conf" 0
    rlRun "cp -v $abrt_action_conf'.backup' $abrt_action_conf" 0
    rlRun "cp -v $retrace_event_conf'.backup' $retrace_event_conf" 0
    rlRun "rm -f $mantis_format_conf" 0

    rlRun "unset RETRACE_SERVER_URI" 0
    rlRun "unset RETRACE_SERVER_PORT" 0
    rlRun "unset RETRACE_SERVER_INSECURE" 0
}

function prepare_crash()
{
    rlRun "rm -f $crash_PATH/reported_to" 0 "Deleting reported to"

    # set component to 'test' because we want to choose $mantis_format_conf
    # as a format conf file for reporter_mantisbt
    rlRun "echo -n 'test' > $crash_PATH/component" 0
}

rlJournalStart
    rlPhaseStartSetup
        LANG=""
        export LANG

        check_prior_crashes

        systemctl start abrtd
        systemctl start abrt-ccpp

        orig_editor=`echo $EDITOR`
        export EDITOR=cat

        TmpDir=$(mktemp -d)
        cp -v $TEST_DIR/queries/* $TmpDir
        cp -v $TEST_DIR/pyserve $TmpDir/pyserve

        cp -v $TEST_DIR/expect $TmpDir/expect

        cp -v $TEST_DIR/fakefaf.py $TmpDir/fakefaf.py

        cp -v $TEST_DIR/retrace.py $TmpDir/retrace.py
        cp -v $TEST_DIR/backtrace $TmpDir/backtrace
        cp -v -r $TEST_DIR/cert $TmpDir

        pushd $TmpDir

        set_configuration
    rlPhaseEnd

    rlPhaseStartTest "testing workflow"
        generate_crash
        wait_for_hooks
        get_crash_path

        prepare_crash

        ./fakefaf.py &
        faf_pid=$!
        wait_for_server $faf_server_port

        ./retrace.py &
        retrace_pid=$!
        wait_for_server $retrace_server_port

        ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/project_get_id_from_name \
                $QUERIES_DIR/search_no_issue \
                $QUERIES_DIR/get_custom_fields \
                $QUERIES_DIR/create \
                $QUERIES_DIR/attachment \
                $QUERIES_DIR/attachment \
                &> server.log &

        mantisbt_pid=$!
        wait_for_server $mantisbt_port

        rlRun "./expect $crash_PATH &> abrt-cli.log" 0 "run abrt-cli report CRASH_DIR"

        kill $faf_pid
        kill $retrace_pid
        kill $mantisbt_pid

        # ureport
        rlAssertGrep "('report_uReport' completed successfully)" abrt-cli.log

        # retrace server
        rlAssertGrep "HTTP/1.0 302 Found" abrt-cli.log
        rlAssertGrep "Preparing an archive to upload" abrt-cli.log
        rlAssertGrep "HTTP/1.0 201 Created" abrt-cli.log
        rlAssertGrep "X-Task-Id: 582841017" abrt-cli.log
        rlAssertGrep "X-Task-Password: OXWE0DJg65NUsR9RGE1zgzG7pBbCYmh9" abrt-cli.log
        rlAssertGrep "X-Task-Status: FINISHED_SUCCESS" abrt-cli.log
        rlAssertGrep "Preparing environment for backtrace generation" abrt-cli.log

        rlAssertGrep "CentOS Bug Tracker User name:" abrt-cli.log
        rlAssertGrep "CentOS Bug Tracker Password:" abrt-cli.log

        rlAssertGrep "Checking for duplicates" abrt-cli.log
        rlAssertGrep "Creating a new issue" abrt-cli.log
        rlAssertGrep "Adding External URL to issue" abrt-cli.log
        rlAssertGrep "Adding attachments to issue 7" abrt-cli.log
        rlAssertGrep "Status: new localhost:12347/view.php?id=7" abrt-cli.log

        rlAssertGrep "Serving at port $mantisbt_port" server.log
        rlAssertGrep "Adding file ./login_correct" server.log
        rlAssertGrep "Adding file ./project_get_id_from_name" server.log

        rlAssertNotGrep "No more files to serve - sending dummy response" server.log

        # log file
        mv -v abrt-cli.log testing-workflow.log
        mv -v server.log testing-workflow-server.log
    rlPhaseEnd

    rlPhaseStartCleanup
        restore_configuration
        rlRun "abrt-cli remove $crash_PATH" 0
        rlBundleLogs abrt testing-workflow.log testing-workflow-server.log
        popd # TmpDir
        rm -rf $TmpDir
        export EDITOR=$orig_editor
        rm -f "/tmp/abrt-done"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
