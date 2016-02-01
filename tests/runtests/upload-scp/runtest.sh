#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of upload-scp
#   Description: Test reporter-upload and scp uploading
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

TEST="upload-scp"
PACKAGE="abrt"
REPORTED_TO=problem_dir/reported_to

rlJournalStart
    rlPhaseStartSetup
        USER_NAME="abrt-scp-user"
        USER_PASS="redhat"
        USER_HOME="/home/${USER_NAME}"
        rlRun "adduser $USER_NAME"
        rlRun "echo $USER_PASS | passwd --stdin $USER_NAME"

        TmpDir=$(mktemp -d)
        cp -R problem_dir $TmpDir
        pushd $TmpDir
        mkdir target
    rlPhaseEnd

    rlPhaseStartTest "scp upload, filename set"
        rlLog "Remove old reported_to file"
        rm -rf $REPORTED_TO

        rlRun "reporter-upload -d problem_dir -u scp://${USER_NAME}:${USER_PASS}@localhost${USER_HOME}/upload.tar.gz"

        rlAssertExists $REPORTED_TO
        cat $REPORTED_TO
        rlAssertEquals "Correct report result" "_upload: URL=scp://localhost${USER_HOME}/upload.tar.gz" "_$(tail -1 $REPORTED_TO)"

        rlAssertExists "${USER_HOME}/upload.tar.gz"
        rm -rf "${USER_HOME}/upload.tar.gz"
    rlPhaseEnd

    rlPhaseStartTest "scp upload, filename not set"
        rlLog "Remove old reported_to file"
        rm -rf $REPORTED_TO

        rlRun "reporter-upload -d problem_dir -u scp://${USER_NAME}:${USER_PASS}@localhost${USER_HOME}/"

        rlAssertExists $REPORTED_TO
        cat $REPORTED_TO
        rlAssertEquals "Correct report result" "_upload: URL=scp://localhost${USER_HOME}/problem_dir.tar.gz" "_$(tail -1 $REPORTED_TO)"

        rlAssertExists ${USER_HOME}/problem_dir*
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "userdel -fr $USER_NAME"

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
