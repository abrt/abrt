#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-dump-journal-core
#   Description: test for abrt-dump-journal-core
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

TEST="abrt-dump-journal-core"
PACKAGE="abrt"
CORE_REQUIRED_FILES="abrt_version analyzer architecture cmdline component core_backtrace count dso_list environ executable hostname kernel last_occurrence limits maps open_fds os_info os_release package pid pkg_arch pkg_epoch pkg_name pkg_release pkg_version reason time type uid username"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        rlRun "systemctl stop abrt-ccpp.service"
        rlRun "systemctl start abrt-journal-core.service"

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "generate crash as root"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        check_dump_dir_attributes $crash_PATH

        for f in $CORE_REQUIRED_FILES; do
            rlAssertExists "$crash_PATH/$f"
        done

        uid=$(cat ${crash_PATH}/uid)
        rlAssertEquals "uid file contains the same id as user who causes the crash" $uid 0
        rlAssertEquals "pid in dumpdir name is the same as in pid element" ${crash_PATH##*-} "$(cat $crash_PATH/pid)"

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "generate crash as abrt-journal-core-test user"
        rlRun "useradd -c \"dump journal core test\" -M abrt_core_test"
        abrt_user_id=$(id -u abrt_core_test)

        prepare
        generate_crash abrt_core_test
        wait_for_hooks
        get_crash_path

        check_dump_dir_attributes $crash_PATH

        for f in $CORE_REQUIRED_FILES; do
            rlAssertExists "$crash_PATH/$f"
        done

        uid=$(cat ${crash_PATH}/uid)
        rlAssertEquals "uid file contains the same id as user who causes the crash" $uid $abrt_user_id
        rlAssertEquals "pid in dumpdir name is the same as in pid element" ${crash_PATH##*-} "$(cat $crash_PATH/pid)"

        rlRun "userdel -r -f abrt_core_test"
        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "systemctl stop abrt-journal-core.service"
        rlRun "systemctl start abrt-ccpp.service"

        rlRun "popd"
        rlLog "$TmpDir"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
