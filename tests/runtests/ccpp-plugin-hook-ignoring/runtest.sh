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

    REASON=${1:-"listed in 'IgnoredPaths'"}
    shift
    USER_PARAM=$1
    user_UID=$(id -u $USER_PARAM)

    prepare
    PID=$(su -c "will_segfault &>/dev/null & echo \$!" $USER_PARAM)
    wait_for_hooks
    # get_crash_path checks if the crash was created
    get_crash_path

    tail -n 10 /var/log/messages >abrt_journal.log

    rlAssertNotGrep "Process $PID \(will_segfault\) of user $user_UID killed by SIGSEGV - ignoring \($REASON\)" abrt_journal.log -E

    rlRun "abrt-cli rm $crash_PATH" 0 "Removing problem dirs"
}

function test_not_create_dir {

    REASON=${1:-"listed in 'IgnoredPaths'"}
    shift
    USER_PARAM=$1
    user_UID=$(id -u $USER_PARAM)

    prepare
    PID=$(su -c "will_segfault &>/dev/null & echo \$!" $USER_PARAM)
    sleep 3

    tail -n 10 /var/log/messages >abrt_journal.log

    rlAssertGrep "Process $PID \(will_segfault\) of user $user_UID killed by SIGSEGV - ignoring \($REASON\)" abrt_journal.log -E

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
        CONF_PATH="/etc/abrt/plugins/CCpp.conf"

        rlLog "Create backup of ${CONF_PATH}"
        rlRun "cp -fv ${CONF_PATH} ${CONF_PATH}.backup"
        rlRun "grep -v IgnoredPaths ${CONF_PATH}.backup > ${CONF_PATH}.without_ignore"

        rlRun "cp -fv ${CONF_PATH}.without_ignore ${CONF_PATH}"
        rlRun "echo \"IgnoredPaths = foo,*will_segfault,foo2\" >> ${CONF_PATH}"

        test_not_create_dir

        rlRun "cp -fv ${CONF_PATH}.without_ignore ${CONF_PATH}"
        rlRun "echo \"IgnoredPaths = foo,/usr/*/will_segfault,foo2\" >> ${CONF_PATH}"

        test_not_create_dir

        rlRun "cp -fv ${CONF_PATH}.without_ignore ${CONF_PATH}"
        rlRun "echo \"IgnoredPaths = foo,/usr/bin/*,foo2\" >> ${CONF_PATH}"

        test_not_create_dir

        rlRun "cp -fv ${CONF_PATH}.without_ignore ${CONF_PATH}"
        rlRun "echo \"IgnoredPaths = foo,/usr/bin/will_*,foo2\" >> ${CONF_PATH}"

        test_not_create_dir

        rlRun "cp -fv ${CONF_PATH}.without_ignore ${CONF_PATH}"
        rlRun "echo \"IgnoredPaths = /usr/bin/will_segfault\" >> ${CONF_PATH}"

        test_not_create_dir

        rlRun "mv -f ${CONF_PATH}.without_ignore $CONF_PATH"
        # will_segfault works without ignoring
        test_create_dir

        rlRun "mv -f ${CONF_PATH}.backup $CONF_PATH"
        rlRun "rm -f ${CONF_PATH}.without_ignore"
    rlPhaseEnd

    rlPhaseStartTest "Ignoring due to either AllowedUsers or AllowedGroups"

        # add user $TEST_USER and add the user to $TEST_GROUP
        TEST_USER=abrt_user_allowed
        TEST_USER2=abrt_user_allowed2
        TEST_GROUP=abrt_group_allowed
        CONF_FILE="/etc/abrt/plugins/CCpp.conf"
        REASON_ALLOW="not allowed in 'AllowedUsers' nor 'AllowedGroups'"

        rlRun "useradd -M $TEST_USER"
        rlRun "useradd -M $TEST_USER2"
        rlRun "groupadd -f $TEST_GROUP"
        rlRun "usermod -aG $TEST_GROUP $TEST_USER"
        rlLog "$(id $TEST_USER)"
        rlLog "$(id $TEST_USER2)"

        rlRun "cp -fv $CONF_FILE ${CONF_FILE}.backup"
        rlRun "grep -v -e AllowedGroups -e AllowedUsers $CONF_FILE > ${CONF_FILE}.no_allowing"
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"

        # will_segfault is processed, if no user allowing is defined
        test_create_dir

        rlLog "===================================================================="
        # $TEST_USER is not allowed
        rlRun "echo \"AllowedUsers = $TEST_USER2,root\" >> ${CONF_FILE}"

        #                   $reason          $username
        test_not_create_dir "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedGroups = $TEST_USER2\" >> ${CONF_FILE}"

        #                   $reason          $username
        test_not_create_dir "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedUsers = $TEST_USER2,root\" >> ${CONF_FILE}"
        rlRun "echo \"AllowedGroups = $TEST_USER2,root\" >> ${CONF_FILE}"

        #                    $reason          $username
        test_not_create_dir  "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        # $TEST_USER is not allowed
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedUsers = $TEST_USER,$TEST_USER2\" >> ${CONF_FILE}"

        #               $reason          $username
        test_create_dir "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedGroups = $TEST_USER2,$TEST_USER\" >> ${CONF_FILE}"

        #               $reason          $username
        test_create_dir "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedGroups = $TEST_USER2,$TEST_GROUP\" >> ${CONF_FILE}"

        #               $reason          $username
        test_create_dir "$REASON_ALLOW"  $TEST_USER

        rlLog "===================================================================="
        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "echo \"AllowedUsers = $TEST_USER2,$TEST_USER\" >> ${CONF_FILE}"
        rlRun "echo \"AllowedGroups = $TEST_USER2,$TEST_USER\" >> ${CONF_FILE}"

        #               $reason          $username
        test_create_dir "$REASON_ALLOW"  $TEST_USER

        rlRun "cp -fv ${CONF_FILE}.no_allowing $CONF_FILE"
        rlRun "rm -f  ${CONF_FILE}.no_allowing"
        rlRun "rm -f  ${CONF_FILE}.backup"

        rlRun "userdel -f $TEST_USER"
        rlRun "userdel -f $TEST_USER2"
        rlRun "groupdel $TEST_GROUP"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
