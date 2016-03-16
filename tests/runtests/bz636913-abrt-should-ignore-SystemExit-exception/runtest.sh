#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz636913-abrt-should-ignore-SystemExit-exception
#   Description: tests if abrt ignores SystemExit exception
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

TEST="bz636913-abrt-should-ignore-SystemExit-exception"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartTest
        CRASHES_BEFORE="$(abrt-cli list | grep Directory | wc -l)"
        rlLog "Crashes before our attempt: $CRASHES_BEFORE"
        rlRun "PYTHONINSPECT=YES dnf repolist <<<quit" 1 "DNF exits with SystemExit=1"

        sleep 1

        CRASHES_AFTER="$(abrt-cli list | grep Directory | wc -l)"
        rlLog "Crashes after our attempt: $CRASHES_BEFORE"
        rlAssertEquals "# of bugs before SystemExit=1 is the same after" $CRASHES_BEFORE $CRASHES_AFTER
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
