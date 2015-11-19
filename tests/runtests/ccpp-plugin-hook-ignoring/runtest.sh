#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-hook-ignoring
#   Description: Test ignoring in abrt-hook-ccpp
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

TEST="ccpp-plugin-hook-ignoring"
PACKAGE="abrt"

function test_create_dir {

    journal_time=$(date +%R:%S)
    prepare
    generate_crash
    wait_for_hooks
    # get_crash_path checks if the crash was created
    get_crash_path

    journalctl --since="$journal_time" >abrt_journal.log

    rlAssertNotGrep "Process [0-9][0-9]* \(will_segfault\) of user [0-9][0-9]* killed by SIGSEGV - ignoring \(listed in 'IgnoredPaths'\)" abrt_journal.log -E

    rlAssertGrep "Process [0-9][0-9]* \(will_segfault\) of user [0-9][0-9]* killed by SIGSEGV - dumping core" abrt_journal.log -E


    rlRun "abrt-cli rm $crash_PATH" 0 "Removing problem dirs"
}

function test_not_create_dir {

    journal_time=$(date +%R:%S)
    prepare
    generate_crash
    sleep 3

    journalctl -b --since="$journal_time" >abrt_journal.log

    rlAssertGrep "Process [0-9][0-9]* \(will_segfault\) of user [0-9][0-9]* killed by SIGSEGV - ignoring \(listed in 'IgnoredPaths'\)" abrt_journal.log -E
    rlAssertNotGrep "Process [0-9][0-9]* \(will_segfault\) of user [0-9][0-9]* killed by SIGSEGV - dumping core" abrt_journal.log -E

    # no crash is generated
    rlAssert0 "Crash should not be generated" $(abrt-cli list 2> /dev/null | wc -l)
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "Ignoring will_segfault"
        OLD_BLACKLIST=$(augtool print /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths | cut -d'=' -f2 | tr -d ' ')
        rlLog "Storing CCpp.conf IgnoredPaths value '$OLD_BLACKLIST'"
        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths foo,*will_segfault,foo2" 0 "Set IgnoredPaths"

        test_not_create_dir

        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths foo,/usr/*/will_segfault,foo2" 0 "Set IgnoredPaths"

        test_not_create_dir

        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths foo,/usr/bin/*,foo2" 0 "Set IgnoredPaths"

        test_not_create_dir
        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths foo,/usr/bin/will_*,foo2" 0 "Set IgnoredPaths"

        test_not_create_dir
        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths /usr/bin/will_segfault" 0 "Set IgnoredPaths"

        test_not_create_dir

        rlRun "augtool rm /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths" 0 "Restore IgnoredPaths option (removing)"
        if [ ! -z $OLD_BLACKLIST ]; then
            rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/IgnoredPaths $OLD_BLACKLIST" 0 "Restore BlackList option (setting old value)"
        fi

        # will_segfault works without ignoring
        test_create_dir
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
