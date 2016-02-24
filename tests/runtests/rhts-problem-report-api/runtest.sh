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

TEST="rhts-problem-report-api"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf

        LANG=""
        export LANG

        CONFIG_FILE="/etc/libreport/plugins/rhtsupport.conf"
        rlFileBackup $CONFIG_FILE
        cp rhtsupport.conf $CONFIG_FILE

        TmpDir=$(mktemp -d)
        cp formatting_files/* "$TmpDir"
        cp -R -- problem_dir "$TmpDir"
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "help"
        rlRun "reporter-rhtsupport --help 2>&1 | grep '[-F FMTFILE]'"
        rlRun "reporter-rhtsupport --help 2>&1 | grep 'Formatting file for a new case'"
    rlPhaseEnd

    rlPhaseStartTest "default formatting string"
        LOG_FILE="default_formatting.log"
        rlRun "reporter-rhtsupport -D -d problem_dir &> $LOG_FILE"

        # remove reporter because it depends on instaled libreport's version
        sed "s/^reporter:.*$//g" ${LOG_FILE} > sed.log
        rlAssertNotDiffer default.right sed.log

        rm sed.log
    rlPhaseEnd

    rlPhaseStartTest "formatting file"
        FORM_FILE="test"
        LOG_FILE="formatting_file.log"
        rlRun "reporter-rhtsupport -D -d problem_dir -F ${FORM_FILE}.conf &> $LOG_FILE"

        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right
    rlPhaseEnd

    rlPhaseStartTest "formatting file does not exist"
        FORM_FILE="i_dont_exist"
        LOG_FILE="formatting_file_dont_exist.log"
        rlRun "reporter-rhtsupport -D -d problem_dir -F ${FORM_FILE}.conf &> $LOG_FILE" 1

        rlAssertGrep "Invalid format file: ${FORM_FILE}.conf" $LOG_FILE
    rlPhaseEnd

    rlPhaseStartTest "formatting file does not exist"
        FORM_FILE="invalid"
        LOG_FILE="invalid_formatting_file.log"
        rlRun "reporter-rhtsupport -D -d problem_dir -F ${FORM_FILE}.conf &> $LOG_FILE" 1

        rlAssertGrep "Invalid format file: ${FORM_FILE}.conf" $LOG_FILE
    rlPhaseEnd

    rlPhaseStartTest "formatting file only summary"
        FORM_FILE="summary"
        LOG_FILE="formatting_file_summary.log"
        rlRun "reporter-rhtsupport -D -d problem_dir -F ${FORM_FILE}.conf &> $LOG_FILE"

        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right
    rlPhaseEnd

    rlPhaseStartTest "formatting file without summary"
        FORM_FILE="without_summary"
        LOG_FILE="formatting_file_without_summary.log"
        rlRun "reporter-rhtsupport -D -d problem_dir -F ${FORM_FILE}.conf &> $LOG_FILE"

        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore #$CONFIG_FILE

        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
