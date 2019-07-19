#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of journal-xorg-crash-processing
#   Description: test for abrt-dump-journal-xorg
#   Author: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="journal-xorg-crash-processing"
PACKAGE="abrt"
XORG_REQUIRED_FILES="kernel backtrace component executable os_info reason time
type uuid duphash pkg_name pkg_arch pkg_epoch pkg_release pkg_version"
EXAMPLES_PATH="../../examples"
SYSLOG_IDENTIFIER="abrt-xorg-test"

function test_single_crash
{
    crash="$1"
    shift
    exe="$1"
    shift
    args=$@

    if [ -z "$crash" ]; then
        rlDie "Need an journal log file as the first command line argument"
    fi

    rlLog "Journal Xorg crash: ${crash}"
    crash_name=${crash%.test}

    if [ -z "$exe" ]; then
        exe="abrt-dump-journal-xorg"
    fi

    prepare

    rlRun "journalctl --flush"

    rlRun "ABRT_DUMP_JOURNAL_XORG_DEBUG_FILTER=\"SYSLOG_IDENTIFIER=${SYSLOG_IDENTIFIER}\" setsid ${exe} ${args} -vvv -f -xD -o >${crash_name}.log 2>&1 &"
    rlRun "ABRT_DUMPER_PID=$!"

    rlRun "sleep 2"
    rlRun "logger -t ${SYSLOG_IDENTIFIER} -f $crash"
    rlRun "journalctl --flush"

    rlRun "sleep 2"

    rlAssertGrep "Found crashes: 1" $crash_name".log"

    wait_for_hooks
    get_crash_path

    ls $crash_PATH > crash_dir_ls

    check_dump_dir_attributes $crash_PATH

    for f in $XORG_REQUIRED_FILES; do
        rlAssertExists "$crash_PATH/$f"
    done

    remove_problem_directory

    # Kill the dumper with TERM to verify that it can store its state.
    # Next time, the dumper should start following the journald from
    # the last seen cursor.
    rlRun "killall -TERM abrt-dump-journal-xorg"
    sleep 2

    if [ -d /proc/$ABRT_DUMPER_PID ]; then
        rlLogError "Failed to kill the abrt journal xorg dumper"
        rlRun "kill -TERM -$ABRT_DUMPER_PID"
    fi

    grep -v "abrt-dump-journal-xorg" ${crash_name}".log" > ${crash_name}".log.right"

    rlRun "diff -u ${crash_name}.right ${crash_name}.log.right" 0 "The dumper copied xorg crash data without any differences"
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cat $EXAMPLES_PATH/xorg_journal_crash1.test \
            | cut -d" " -f6- > \
            $TmpDir/xorg_journal_crash1.test
        cp -v $EXAMPLES_PATH/xorg_journal_crash1.right $TmpDir/

        cp -v $EXAMPLES_PATH/xorg_journal_crash2.test $TmpDir/
        cp -v $EXAMPLES_PATH/xorg_journal_crash2.right $TmpDir/

        pushd $TmpDir

        rlRun "systemctl stop abrt-xorg"

        # The stored cursor is not valid in testing configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dump-journal-xorg.state"
    rlPhaseEnd

    rlPhaseStartTest "Xorg crashes"
        for crash in xorg*.test; do
            test_single_crash ${crash}
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        # Do not confuse the system dumper. The stored cursor is invalid in the default configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dump-journal-xorg.state"

        rlBundleLogs abrt $(echo *_ls) $(echo *.log)
        rlRun "popd"
        rlLog "$TmpDir"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
