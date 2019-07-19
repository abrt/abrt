#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of mailx-dead-letter
#   Description: Verify that dead.letter is not created in case of fail
#   Author: Matej Habrnal <mhabrnal@redhat.com>
# # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2016 Red Hat, Inc. All rights reserved.
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

TEST="mailx-dead-letter"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        MAILX_EVENT_FILE="mailx_event.conf"
        MAILX_EVENT_PATH="/etc/libreport/events.d/$MAILX_EVENT_FILE"
        rlRun "mv $MAILX_EVENT_PATH ${MAILX_EVENT_PATH}.backup"
        cp -fv $MAILX_EVENT_FILE $MAILX_EVENT_PATH

        TmpDir=$(mktemp -d)
        cp mailx.conf $TmpDir
        rlRun "pushd $TmpDir"
    rlPhaseEnd

    rlPhaseStartTest
        # missing sendmail causes that mailx fails
        SENDMAIL=$(which sendmail)
        rlFileBackup $SENDMAIL
        rlRun "mv -f $SENDMAIL ${SENDMAIL}.backup"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "ls $crash_PATH > dir_content.log"

        rlRun "reporter-mailx -d $crash_PATH -c mailx.conf" 1

        rlAssertNotExists "${crash_PATH}/dead.letter"
        rlRun "ls $crash_PATH > dir_content_after.log"

        rlAssertNotDiffer dir_content.log dir_content_after.log
        rlLog $(diff dir_content.log dir_content_after.log)

        rlFileRestore #SENDMAIL
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "mv ${MAILX_EVENT_PATH}.backup $MAILX_EVENT_PATH"
        remove_problem_directory
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
