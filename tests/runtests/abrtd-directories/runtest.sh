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

        systemctl stop abrtd
        rlRun "rm -rf $ABRT_CONF_DUMP_LOCATION" "0" "Prepare to force abrtd to create the dump location at startup"
        RECREATION_CNT=$( grep -c "Recreating deleted dump location '$ABRT_CONF_DUMP_LOCATION'" /var/log/messages )

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest
        # abrtd creates the dump location
        rlRun "systemctl restart abrtd"
        rlRun "systemctl restart abrt-ccpp.service"

        rlAssertExists "$ABRT_CONF_DUMP_LOCATION"

        rlRun "rm -rf -- $ABRT_CONF_DUMP_LOCATION" "0" "Remove the dump location for 1st time"
        sleep 1
        rlAssertEquals "abrtd recreated the dump location once" "_"$((RECREATION_CNT + 1)) "_"$(grep -c "Recreating deleted dump location '$ABRT_CONF_DUMP_LOCATION'" /var/log/messages)
        rlAssertExists $ABRT_CONF_DUMP_LOCATION

        rlRun "rm -rf -- $ABRT_CONF_DUMP_LOCATION" "0" "Remove the dump location for 2nd time"
        sleep 1
        rlAssertEquals "abrtd recreated the dump location twice" "_"$((RECREATION_CNT + 2)) "_"$(grep -c "Recreating deleted dump location '$ABRT_CONF_DUMP_LOCATION'" /var/log/messages)
        rlAssertExists "$ABRT_CONF_DUMP_LOCATION"

        # check if inotify works and abrtd will find a new dump directory
        will_segfault
        wait_for_hooks
        get_crash_path

    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
