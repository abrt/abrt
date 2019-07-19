#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-should-return-rating-0-on-fail
#   Description: tests whether abrt returns zero on backtrace generation fail
#   Author: Michal Nowak <mnowak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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

TEST="abrt-should-return-rating-0-on-fail"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        generate_crash
        wait_for_hooks
        get_crash_path

        rlLog "PATH = $crash_PATH"
    rlPhaseEnd
    rlPhaseStartTest
        rlRun "abrt-action-generate-backtrace -d ${crash_PATH}" 0 "Backtrace generated"
        rlRun "abrt-action-analyze-backtrace -d ${crash_PATH}" 0 "Rating generated"
        if [ ! -e ${crash_PATH}/backtrace_rating ]; then
            rlLog "$(tail /var/log/messages)";
            rlDie "File 'backtrace_rating' in $crash_PATH does not exist";
        fi
        RATING_N="$(cat ${crash_PATH}/backtrace_rating)"
        if [ -z "${RATING_N}" ]; then
                rlLogError "RATING EMPTY -> FAIL"
        else
                rlLog "RATING=$RATING_N"
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        remove_problem_directory
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd

