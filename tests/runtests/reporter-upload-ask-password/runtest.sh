#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of reporter-upload-ask-password
#   Description: Test reporter-upload password asking
#   Author: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="reporter-upload-ask-password"
PACKAGE="abrt"
REPORTED_TO=problem_dir/reported_to

function test()
{
    NUM=$1
    shift
    LOG=$1

    rlLog "Testing wrong credentials $NUM times"

    rlRun "./expect $user_name $NUM &>$LOG"

    rlAssertNotGrep "Please enter password for uploading:" $LOG
    rlAssertGrep "Please enter user name for 'scp://localhost': $user_name" $LOG
    rlAssertGrep "Please enter password for 'scp://$user_name@localhost':" $LOG

    rlAssertExists upload.tar.gz

    # tarball contains all files from problem_dir
    rlRun "mkdir recieved_dir"
    rlRun "tar zxvf upload.tar.gz -C recieved_dir"

    rlRun "rm $REPORTED_TO" 0 "remove reported_to created after upload"
    rlRun "diff -r problem_dir recieved_dir &> dir_diff.log"
    rlLog $(cat dir_diff.log)

    rlRun "rm -f upload.tar.gz"
    rlRun "rm -rf recieved_dir"
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -R problem_dir $TmpDir
        cp ./expect $TmpDir
        pushd $TmpDir
        mkdir target

        event_upload_conf="/etc/libreport/events/report_Uploader.conf"
        rlFileBackup $event_upload_conf

        user_name="abrt_ask_passwd"
        rlRun "useradd $user_name"
        if ! id $user_name; then
            rlDie "Create user $user_name failed"
        fi

        rlRun "cp -R problem_dir /home/$user_name/"
        rlRun "cp ./expect /home/$user_name/"

        pushd /home/$user_name
        rlRun "chown -R ${user_name}.${user_name} problem_dir"
        rlRun "chown ${user_name}.${user_name} expect"
        echo "redhat" | passwd $user_name --stdin
    rlPhaseEnd

    rlPhaseStartTest "reporter-upload should ask for passwd if only Upload_Username is set"
        # if Upload_Username is set and Upload_Password is not set,
        # reporter-upload should ask for password
cat > $event_upload_conf <<EOF
Upload_URL = scp://$user_name@localhost/home/$user_name/upload.tar.gz
Upload_Username = $user_name
# password is not set in the conf file
#Upload_Password = foo
EOF
        LOG_FILE="ask_password.log"
        rlRun "./expect $user_name 1 &>$LOG_FILE"

        # reporter-upload asks for password
        # expect fills a wrong password, so curl library ask again
        rlAssertGrep "Please enter password for uploading:" $LOG_FILE

        rlAssertGrep "Please enter user name for 'scp://localhost': $user_name" $LOG_FILE
        rlAssertGrep "Please enter password for 'scp://$user_name@localhost':" $LOG_FILE

        rlAssertExists upload.tar.gz
        rlRun "rm -f upload.tar.gz"
        rlRun "rm -f $REPORTED_TO" 0 "remove reported_to created after upload"
    rlPhaseEnd

    rlPhaseStartTest "authentication by credentials"
        # wrong credentials are set to the event's conf file,
        # so the curl library will ask again
cat > $event_upload_conf <<EOF
Upload_URL = scp://$user_name@localhost/home/$user_name/upload.tar.gz
Upload_Username = username
Upload_Password = passwd
EOF
        # wrong password is set to curl $i times
        for i in {0..3}; do
            test $i "ask_password_${i}.log"
        done

    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "cp -v *.log $TmpDir"
        popd # /home/$user_name
        rlRun "userdel -f -r $user_name"

        rlFileRestore # $event_upload_conf
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
