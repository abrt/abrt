#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrtd-directories
#   Description: Tests ability to recreate essentials directories of abrtd
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
#
#   This copyrighted material is made available to anyone wishing
#   to use, modify, copy, or redistribute it subject to the terms
#   and conditions of the GNU General Public License version 2.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE. See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public
#   License along with this program; if not, write to the Free
#   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="abrtd-directories"
PACKAGE="abrt"




rlJournalStart
    rlPhaseStartSetup
        prepare

        rlServiceStop abrtd
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION" "0" "Prepare to force abrtd to create the dump location at startup"

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd
    rlPhaseStartTest
        # abrtd creates the dump location
        rlServiceStart abrtd abrt-journal-core

        rlAssertExists "$ABRT_CONF_DUMP_LOCATION"
        rlAssertEquals "Dump location has proper stat" "_$(stat --format='%A %U %G' $ABRT_CONF_DUMP_LOCATION)" "_drwxr-x--x root abrt"

        SINCE=$(date +"%Y-%m-%d %T")
        rlRun "rm -rf -- $ABRT_CONF_DUMP_LOCATION" "0" "Remove the dump location for 1st time"
        sleep 1
        journalctl SYSLOG_IDENTIFIER=abrtd --since="$SINCE" | tee recreate_first_time.log
        rlAssertGrep "Recreating deleted dump location '$ABRT_CONF_DUMP_LOCATION'" recreate_first_time.log
        rlAssertExists $ABRT_CONF_DUMP_LOCATION
        rlAssertEquals "Dump location has proper stat" "_$(stat --format='%A %U %G' $ABRT_CONF_DUMP_LOCATION)" "_drwxr-x--x root abrt"

        sleep 1
        SINCE=$(date +"%Y-%m-%d %T")
        rlRun "rm -rf -- $ABRT_CONF_DUMP_LOCATION" "0" "Remove the dump location for 2nd time"
        sleep 1
        journalctl SYSLOG_IDENTIFIER=abrtd --since="$SINCE" | tee recreate_second_time.log
        rlAssertGrep "Recreating deleted dump location '$ABRT_CONF_DUMP_LOCATION'" recreate_second_time.log
        rlAssertExists "$ABRT_CONF_DUMP_LOCATION"
        rlAssertEquals "Dump location has proper stat" "_$(stat --format='%A %U %G' $ABRT_CONF_DUMP_LOCATION)" "_drwxr-x--x root abrt"

        # check if inotify works and abrtd will find a new dump directory
        will_segfault
        wait_for_hooks
        get_crash_path

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "change DumpLocation"
        NEW_ABRT_CONF_DUMP_LOCATION="/var/spool/abrt-test"

        rlServiceStart abrtd abrt-journal-core

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # dump dir was created in $ABRT_CONF_DUMP_LOCATION
        rlRun "[[ _$crash_PATH == _"$ABRT_CONF_DUMP_LOCATION"* ]]"
        rlRun "[[ _$crash_PATH != "_$NEW_ABRT_CONF_DUMP_LOCATION"* ]]"

        remove_problem_directory

        # set a new dump directory location
        rlRun "augtool set /files/etc/abrt/abrt.conf/DumpLocation $NEW_ABRT_CONF_DUMP_LOCATION"

        rlServiceStart abrtd abrt-journal-core

        # generate a new crash
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # dump dir was created in $NEW_ABRT_CONF_DUMP_LOCATION
        rlRun "[[ _$crash_PATH == _"$NEW_ABRT_CONF_DUMP_LOCATION"* ]]"

        remove_problem_directory

        # set a dump directory location to default
        rlRun "augtool rm /files/etc/abrt/abrt.conf/DumpLocation"

        rlServiceStart abrtd abrt-journal-core

        # generate a new crash
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # dump dir was created in $ABRT_CONF_DUMP_LOCATION
        rlRun "[[ _$crash_PATH == "_$ABRT_CONF_DUMP_LOCATION"* ]]"
        rlRun "[[ _$crash_PATH != "_$NEW_ABRT_CONF_DUMP_LOCATION"* ]]"

        rlRun "rm -rf $NEW_ABRT_CONF_DUMP_LOCATION" 0 "Remove crash directory"
        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
