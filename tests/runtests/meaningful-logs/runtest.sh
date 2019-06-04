#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of meaningful-logs
#   Description: The test verifies that abrt do not spam system logs
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

TEST="meaningful-logs"
PACKAGE="abrt"

function capture_abrtd_startup_logs
{
    local SINCE=$(date +"%Y-%m-%d %T")

    journalctl SYSLOG_IDENTIFIER=abrtd --since="$SINCE" > $1-start

    rlRun "rm -rf /var/run/abrt/abrtd.pid"
    rlRun "systemctl start abrtd"

    local TO=500
    local CNT=0
    while [ $CNT -lt $TO ]
    do
        if [ -e /var/run/abrt/abrtd.pid ]; then
            break
        fi

        sleep 0.1
        ((CNT++))
    done

    journalctl SYSLOG_IDENTIFIER=abrtd --since="$SINCE" > $1

    rlRun "systemctl stop abrtd"
    rlRun "systemctl reset-failed abrtd.service"

    if [ $CNT -eq $TO ]; then
        rlLog "Capturing logs timed out"
        return 1
    fi

    return 0
}

function missing_or_corrupted_time
{
    local LOG_NAME=$1_time_file.log
    capture_abrtd_startup_logs $LOG_NAME

    if [ $? -eq 0 ]
    then
        rlAssertNotGrep "Missing file: time" $LOG_NAME
        rlAssertNotGrep "Unlocked '.*' (no or corrupted 'time' file)" $LOG_NAME
        rlAssertGrep "'/var/.*/abrt/.*' is not a problem directory" $LOG_NAME
        rlAssertGreaterOrEqual "Sane number of lines" "03" "0$(count_new_lines $LOG_NAME)"
    else
        rlFail "Could not capture abrtd logs for $1 time file"
    fi
}

function count_new_lines
{
    echo $(( $(wc -l $1 | cut -f 1 -d ' ') - $(wc -l $1-start | cut -f 1 -d ' ') ))
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        load_abrt_conf

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "abrtd - start up"

        rlRun "systemctl stop abrtd"

        crashPATH="$ABRT_CONF_DUMP_LOCATION/testsuite-sane-logs"

        sleep 1.1
        rlRun "mkdir $crashPATH"
        missing_or_corrupted_time missing

        sleep 1.1
        rlRun "touch $crashPATH/time"
        missing_or_corrupted_time empty

        sleep 1.1
        rlRun "echo -n foo > $crashPATH/time"
        missing_or_corrupted_time invalid

        sleep 1.1
        rlRun "echo -n 12345678 > $crashPATH/time"
        capture_abrtd_startup_logs time_file_exists.log

        if [ $? -eq 0 ]; then
            rlAssertNotGrep "Can't open file 'type': No such file or directory" time_file_exists.log
            rlAssertNotGrep "Missing or empty file: type" time_file_exists.log
            rlAssertNotGrep "Unlocked '.*' (no or corrupted 'type' file)" time_file_exists.log
            rlAssertGrep "'/var/.*/abrt/.*' is not a problem directory" time_file_exists.log
            rlAssertGreaterOrEqual "Sane number of line" "03" "0$(count_new_lines time_file_exists.log)"
        else
            rlFail "Could not capture abrtd logs for a directory with 'time' file"
        fi

        sleep 1.1
        rlRun "echo -n testsuite > $crashPATH/type"
        capture_abrtd_startup_logs type_time_files_exist.log

        if [ $? -eq 0 ]; then
            rlAssertGrep "Marking '/var/.*/abrt/.*' not reportable (no 'count' item)" type_time_files_exist.log
            rlAssertGreaterOrEqual "Sane number of line" "03" "0$(count_new_lines type_time_files_exist.log)"
        else
            rlFail "Could not capture abrtd logs for a directory with 'time' & 'type' files"
        fi

        # No broken dump directories, no log messages
        sleep 1.1
        rlRun "echo -n 1 > $crashPATH/count"
        capture_abrtd_startup_logs dump_directory_with_count.log

        if [ $? -eq 0 ]; then
            rlAssertGreaterOrEqual "Sane number of line" "03" "0$(count_new_lines dump_directory_with_count.log)"
        else
            rlFail "Could not capture abrtd logs for a directory with 'time' & 'type' & 'count' files"
        fi

        rlRun "rm -rf $crashPATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt-meaningful-logs $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
