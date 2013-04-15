#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of upload-ftp
#   Description: Test reporter-upload and ftp uploading
#   Author: Richard Marko <rmarko@redhat.com>
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
. ../aux/lib.sh

TEST="upload-ftp"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp ftpserver.py $TmpDir
        cp -R problem_dir $TmpDir
        pushd $TmpDir
        mkdir target
        python ftpserver.py --directory=$TmpDir/target --port=2121 --write &
        sleep 2
    rlPhaseEnd

    rlPhaseStartTest "ftp upload, filename set"
        rlRun "reporter-upload -d problem_dir -u ftp://localhost:2121/upload.tar.gz"
        rlAssertExists "$TmpDir/target/upload.tar.gz"
        rm -rf "$TmpDir/target/upload.tar.gz"
    rlPhaseEnd

    rlPhaseStartTest "ftp upload, filename not set"
        rlRun "reporter-upload -d problem_dir -u ftp://localhost:2121/"
        rlAssertExists $TmpDir/target/problem_dir*
    rlPhaseEnd

    rlPhaseStartCleanup
        kill %1 # ftp server
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
