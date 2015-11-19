#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of augeas
#   Description: Test abrt and libreport augeas lenses sanity
#   Author: Jakub Filak <jfilak@redhat.com>
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

TEST="augeas"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        pushd $TmpDir

        if ! which augtool; then
            dnf install -y augeas
        fi
    rlPhaseEnd

    rlPhaseStartTest "libreport configuration files"
        rlRun "augtool print /augeas/files/etc/libreport//error 2>&1 | tee libreport_error.log"
        rlAssertEquals "No errors" "_0 libreport_error.log" "_$(wc -l libreport_error.log)"

        rlRun "augtool print /files/etc/libreport/ > libreport.log"
        rlAssertNotEquals "Parsed files" "_0 libreport.log" "_$(wc -l libreport.log)"
    rlPhaseEnd

    rlPhaseStartTest "abrt configuration files"
        rlRun "augtool print /augeas/files/etc/abrt//error 2>&1 | tee abrt_error.log"
        rlAssertEquals "No errors" "_0 abrt_error.log" "_$(wc -l abrt_error.log)"

        rlRun "augtool print /files/etc/abrt/ > abrt.log"
        rlAssertNotEquals "Parsed files" "_0 abrt.log" "_$(wc -l abrt.log)"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs augeas $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
