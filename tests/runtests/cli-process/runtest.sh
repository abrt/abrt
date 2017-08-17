#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   PURPOSE of cli-process
#   Description: Check abrt-cli process funcionality
#   Autor: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="cli-process"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup

        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp "./expect" $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "process"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "./expect skip &> process.log" 0 "Running abrt-cli process via expect"

        rlAssertGrep "reason:" process.log
        rlAssertGrep "time:" process.log
        rlAssertGrep "cmdline:" process.log
        rlAssertGrep "Directory:" process.log
        rlAssertGrep "$crash_PATH" process.log
        rlAssertGrep "Actions: remove(rm), report(e), info(i), skip(s):" process.log

    rlPhaseEnd

    rlPhaseStartTest "process action info"

        #abrt-cli info $crash_PATH
        rlRun "./expect info &> process-info.log" 0 "Running abrt-cli info via expect"

        rlAssertGrep "reason:" process-info.log
        rlAssertGrep "time:" process-info.log
        rlAssertGrep "cmdline:" process-info.log
        rlAssertGrep "Directory:" process-info.log
        rlAssertGrep "$crash_PATH" process-info.log
        rlAssertGrep "Actions: remove(rm), report(e), info(i), skip(s):" process-info.log

        rlRun "abrt-cli info -d $crash_PATH &> info.log"
        rlRun "tail -$(wc -l info.log | cut -f1 -d' ') process-info.log | tr -d '\r' > process-info.tmp"
        rlAssertNotDiffer info.log process-info.tmp

    rlPhaseEnd

    rlPhaseStartTest "process not-reportable"

        #create not-reportable file
        rlRun "echo not-reportable > $crash_PATH/not-reportable" 0 "Creating not-reportable file"

        #abrt-cli skip $crash_PATH
        rlRun "./expect skip &> notrep.log" 0 "Running abrt-cli skip via expect"

        rlAssertGrep "reason:" notrep.log
        rlAssertGrep "time:" notrep.log
        rlAssertGrep "cmdline:" notrep.log
        rlAssertGrep "Directory:" notrep.log
        rlAssertGrep "$crash_PATH" notrep.log
        rlAssertGrep "Actions: remove(rm), info(i), skip(s):" notrep.log

        rlRun "rm -f $crash_PATH/not-reportable" 0 "Removing not-reportable file"

        #abrt-cli skip $crash_PATH
        rlRun "./expect skip &> notrep.log" 0 "Running abrt-cli skip via expect"

        rlAssertGrep "reason:" notrep.log
        rlAssertGrep "time:" notrep.log
        rlAssertGrep "cmdline:" notrep.log
        rlAssertGrep "Directory:" notrep.log
        rlAssertGrep "$crash_PATH" notrep.log
        rlAssertGrep "Actions: remove(rm), report(e), info(i), skip(s):" notrep.log

    rlPhaseEnd

    rlPhaseStartTest "process action report"

        #set testing analyzer to problem
        rlAssertExists "$crash_PATH/analyzer"
        rlAssertExists "$crash_PATH/type"
        echo -n ABRT_testing > $crash_PATH/analyzer
        echo -n ABRT_testing > $crash_PATH/type
        rlAssertGrep "ABRT_testing" $crash_PATH/analyzer
        rlAssertGrep "ABRT_testing" $crash_PATH/type

        workflow_conf_file="/etc/libreport/workflows.d/report_testing.conf"
        cat > $workflow_conf_file << EOF
EVENT=workflow_testing analyzer=ABRT_testing type=ABRT_testing
EOF
        rlAssertExists "$workflow_conf_file"

        workflow_file="/usr/share/libreport/workflows/workflow_testing.xml"
        cat > $workflow_file << EOF
<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<workflow>
    <name>Report testing</name>
    <description>Testing workflow for reporting</description>
    <priority>-99</priority>
    <events>
        <event>report_testing</event>
    </events>
</workflow>
EOF

        rlAssertExists "$workflow_file"

        event_conf_file="/etc/libreport/events.d/testing_event.conf"
        cat > $event_conf_file << EOF
EVENT=report_testing analyzer=ABRT_testing type=ABRT_testing
    echo REPORTING

EVENT=report-cli analyzer=ABRT_testing type=ABRT_testing
    report-cli -- "\$DUMP_DIR"
EOF

        event_file="/usr/share/libreport/events/report_testing.xml"
        cat > $event_file << EOF
<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<event>
    <name>Testing report</name>
    <description>Testing event report_testing</description>
    <gui-review-elements>no</gui-review-elements>
</event>
EOF

        rlAssertExists "$event_conf_file"

        rlRun "./expect report &> process_report.log" 0 "Running abrt-cli reporting via expect"
        rlRun "abrt-cli e $crash_PATH &> report.log"

        rlAssertGrep "$(cat report.log)" process_report.log

        rlRun "rm -f $workflow_conf_file $workflow_file $event_conf_file $event_file" 0 "removing configuration file"

    rlPhaseEnd

    rlPhaseStartTest "process action report not-reportable as unsafe"

        #set testing analyzer to problem
        rlAssertExists "$crash_PATH/analyzer"
        rlAssertExists "$crash_PATH/type"
        echo -n ABRT_testing > $crash_PATH/analyzer
        echo -n ABRT_testing > $crash_PATH/type
        rlAssertGrep "ABRT_testing" $crash_PATH/analyzer
        rlAssertGrep "ABRT_testing" $crash_PATH/type

        rlRun "echo not-reportable > $crash_PATH/not-reportable" 0 "Creating not-reportable file"

        workflow_conf_file="/etc/libreport/workflows.d/report_testing.conf"
        cat > $workflow_conf_file << EOF
EVENT=workflow_testing analyzer=ABRT_testing type=ABRT_testing
EOF
        rlAssertExists "$workflow_conf_file"

        workflow_file="/usr/share/libreport/workflows/workflow_testing.xml"
        cat > $workflow_file << EOF
<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<workflow>
    <name>Report testing</name>
    <description>Testing workflow for reporting</description>
    <priority>-99</priority>
    <events>
        <event>report_testing</event>
    </events>
</workflow>
EOF

        rlAssertExists "$workflow_file"

        event_conf_file="/etc/libreport/events.d/testing_event.conf"
        cat > $event_conf_file << EOF
EVENT=report_testing analyzer=ABRT_testing type=ABRT_testing
    echo REPORTING

EVENT=report-cli analyzer=ABRT_testing type=ABRT_testing
    report-cli -- "\$DUMP_DIR"
EOF

        event_file="/usr/share/libreport/events/report_testing.xml"
        cat > $event_file << EOF
<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<event>
    <name>Testing report</name>
    <description>Testing event report_testing</description>
    <gui-review-elements>no</gui-review-elements>
</event>
EOF

        rlAssertExists "$event_conf_file"

        rlRun "./expect report --unsafe &> process_report_unsafe.log" 0 "Running abrt-cli reporting via expect"
        rlRun "abrt-cli e --unsafe $crash_PATH &> report_unsafe.log"

        rlAssertGrep "$(cat report_unsafe.log)" process_report_unsafe.log

        rlRun "rm -f $workflow_conf_file $workflow_file $event_conf_file $event_file" 0 "removing configuration file"
        rlRun "rm -f  $crash_PATH/not-reportable" 0 "removing not-reportable file"

    rlPhaseEnd


    rlPhaseStartTest "process action remove"

        rlAssertExists "$crash_PATH"

        #abrt-cli remove
        rlRun "./expect remove" 0 "Running abrt-cli remove via expect"

        rlAssertNotExists "$crash_PATH"

    rlPhaseEnd

    rlPhaseStartTest "process --since"
        prepare
        # abrt-ccpp ignores repeated crashes of a single executable
        generate_python_segfault
        wait_for_hooks
        get_crash_path

        sleep 2
        prepare
        generate_second_crash
        wait_for_hooks

        rlAssertGreater "Second crash recorded" $(abrt-cli list | wc -l) 0
        crash2_PATH="$(abrt-cli list | grep Directory \
            | grep -v "$crash_PATH" \
            | awk '{ print $2 }' | tail -n1)"
        if [ ! -d "$crash2_PATH" ]; then
            rlDie "No crash dir generated for second crash, this shouldn't happen"
        fi

        second_crash_last_occurrence=`cat $crash2_PATH/last_occurrence`

        rlLog "PATH2 = $crash2_PATH"

        rlRun "./expect skip --since $second_crash_last_occurrence &> since.log" 0 "Running abrt-cli skip --since via expect"

        rlAssertGrep "$crash2_PATH" since.log
        rlAssertNotGrep "$crash_PATH" since.log

        rlRun "abrt-cli rm $crash_PATH $crash2_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt-cli-process $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
