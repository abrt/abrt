#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of upload-handling
#   Description: Test upload handling (central crash collecting)
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

TEST="upload-handling"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlFileBackup /etc/abrt/abrt.conf
        TmpDir=$(mktemp -d)
        cp -R problem_dir $TmpDir
        pushd $TmpDir
        echo "WatchCrashdumpArchiveDir = /var/spool/abrt-upload/" > /etc/abrt/abrt.conf
        rlRun "setsebool -P abrt_anon_write 1"
        rlRun "service abrtd stop" 0 "Killing abrtd"
        rlRun "service abrtd start" 0 "Starting abrtd"
        rlRun "service abrt-ccpp restart" 0 "Start abrt-ccpp"
    rlPhaseEnd

    rlPhaseStartTest "handle upload"
        sleep 3
        rlRun "reporter-upload -d problem_dir -u file:///var/spool/abrt-upload/upload.tar.gz"
        sleep 3

        rem_upload_dir=$( echo /var/spool/abrt/remote* )
        rlAssertExists "$rem_upload_dir/coredump"
        rlAssertNotExists "/var/spool/abrt-upload/upload.tar.gz"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
        rm -rf $rem_upload_dir
        rm -f "/var/spool/abrt-upload/upload.tar.gz"
        rlFileRestore
        rlRun "service abrtd stop" 0 "Killing abrtd"
        rlRun "service abrtd start" 0 "Starting abrtd"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
