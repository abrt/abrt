#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#   runtest.sh of pstoreoops
#   Description: Test abrt-merge-pstoreoops
#   Author: Denys Vlasenko <dvlasenk@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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

TEST="pstoreoops"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -R -- dmesg-efi-* "$TmpDir"
        pushd -- "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "merge pstore oops"
        rlRun "abrt-merge-pstoreoops -o dmesg-efi-* | grep 'Process Xorg'" 0 "Testing merging"
    rlPhaseEnd

    rlPhaseStartTest "delete pstore oops"
        rlRun "abrt-merge-pstoreoops -d dmesg-efi-*" 0 "Testing deleting"
        rlAssertNotExists dmesg-efi-1
        rlAssertNotExists dmesg-efi-2
    rlPhaseEnd

    rlPhaseStartCleanup
        popd
        rm -rf -- "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
