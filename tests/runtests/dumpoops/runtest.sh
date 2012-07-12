#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dumpoops
#   Description: looks for oops via dumpoops
#   Author: Michal Nowak <mnowak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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

TEST="dumpoops"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        tar xf examples.tar -C $TmpDir
        rlRun "pushd $TmpDir/examples"
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            rlRun "abrt-dump-oops $oops 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "[$oops] Found OOPS"
        done
    rlPhaseEnd

    rlPhaseStartTest not-OOPS
        for noops in not_oops*.test; do
            rlRun "abrt-dump-oops $noops 2>&1 | grep 'abrt-dump-oops: Found oopses: 0'" 0 "[$noops] Not found OOPS"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
