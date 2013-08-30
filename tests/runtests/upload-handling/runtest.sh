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
. ../aux/lib.sh

TEST="upload-handling"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlFileBackup /etc/abrt/abrt.conf
        TmpDir=$(mktemp -d)
        cp -R problem_dir $TmpDir
        pushd $TmpDir
        mkdir -p /var/spool/abrt-upload/
        echo "WatchCrashdumpArchiveDir = /var/spool/abrt-upload/" > /etc/abrt/abrt.conf
        load_abrt_conf
        # the upload watcher is not installed by default, but we need it for this test
        # but it's not available on rhel6, so don't fail!
        # the upload watcher is not installed by default, but we need it for this test
        upload_watch_pkg="abrt-addon-upload-watch"
        rlRun "rpm -q $upload_watch_pkg >/dev/null || (yum install $upload_watch_pkg -y || :)"
        rlRun "rpm -q $upload_watch_pkg >/dev/null || (echo 'WARN $upload_watch_pkg is not available, are we on rhel6?'; :)"
        rlRun "setsebool -P abrt_anon_write 1"
        rlRun "service abrtd stop" 0 "Killing abrtd"
        rlRun "service abrtd start" 0 "Starting abrtd"
        rlRun "service abrt-upload-watch restart" 0 "Starting abrt-upload-watch"
        rlRun "service abrt-ccpp restart" 0 "Start abrt-ccpp"
    rlPhaseEnd

    rlPhaseStartTest "handle upload"
        rlRun "reporter-upload -d problem_dir -u file:///var/spool/abrt-upload/upload.tar.gz"

        wait_for_hooks

        rem_upload_dir=$( echo $ABRT_CONF_DUMP_LOCATION/remote* )
        rlAssertExists "$rem_upload_dir/coredump"
        # because of commit c43d2e7b890e48fd30e248f2d578f4bde81cc140 and rhbz#839285
        rlAssertExists "/var/spool/abrt-upload/upload.tar.gz"
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
