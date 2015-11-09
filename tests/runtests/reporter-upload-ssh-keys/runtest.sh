#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of reporter-upload-ssh-keys
#   Description: Test reporter-upload SSH options
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

TEST="reporter-upload-ssh-keys"
PACKAGE="abrt"

# uploads problem_dir and checks if the dir is uploaded correctly
function test() {

    PARAMS=$1

    LOG_FILE="ssh_upload.log"
    rlRun "sudo -u $user_name ./expect reporter-upload -vvv -d problem_dir -u scp://$user_name@localhost/home/$user_name/upload.tar.gz $PARAMS >$LOG_FILE 2>&1"

    rlAssertExists upload.tar.gz
    rlRun "mkdir recieved_dir"
    rlRun "tar zxvf upload.tar.gz -C recieved_dir"

    rlRun "diff -r problem_dir recieved_dir &> dir_diff.log"
    rlLog $(cat dir_diff.log)

    rlRun "rm -rf recieved_dir"
    rlRun "rm -rf upload.tar.gz"
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -R problem_dir $TmpDir
        cp ./expect $TmpDir
        cp -R ssh_keyfiles $TmpDir
        pushd $TmpDir
        mkdir target

        # because of report-cli's private data review
        editor_var=$(echo $EDITOR)
        rlRun "export EDITOR=cat"
    rlPhaseEnd

    rlPhaseStartTest "test scp - SSH keyfiles by command line arguments"
        user_name="abrt_ssh_test"
        rlRun "useradd $user_name"
        if ! id $user_name; then
            rlDie "Create user $user_name failed"
        fi

        rlRun "cp -R ssh_keyfiles /home/$user_name/.ssh"
        rlRun "cp -R problem_dir /home/$user_name/"
        rlRun "cp ./expect /home/$user_name/"

        pushd /home/$user_name
        rlRun "chown -R ${user_name}.${user_name} .ssh"
        rlRun "chown -R ${user_name}.${user_name} problem_dir"
        rlRun "chown ${user_name}.${user_name} expect"

        # use ssh keys from standard dir (/home/$user_name/.ssh)
        test
        rlRun "mv -fv .ssh/id_rsa private_key"
        rlRun "mv -fv .ssh/id_rsa.pub public_key"
        # remove the other key file
        rlRun "rm -f .ssh/id_dsa"
        rlRun "rm -f .ssh/id_dsa.pub"
        # use ssh keys from nonstandard dir (/home/$user_name/)
        test "-r private_key -b public_key"

        popd # /home/$user_name
        rlRun "userdel -f -r $user_name"
    rlPhaseEnd

    rlPhaseStartTest "set SSH keyfile by command line arguments"
        # the ssh keys are set by command line argument

        LOG_FILE="ssh_commandline.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz -b pub-commandline -r pri-commandline >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-commandline'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-commandline'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-commandline" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-commandline" $LOG_FILE

        LOG_FILE="ssh_commandline_only_public.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz -b pub-commandline >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-commandline'" $LOG_FILE
        rlAssertNotGrep "Using SSH private key 'pri-commandline'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-commandline" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh private key file pri-commandline" $LOG_FILE

        LOG_FILE="ssh_commandline_only_private.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz -r pri-commandline >$LOG_FILE 2>&1"

        rlAsserNottGrep "Using SSH public key 'pub-commandline'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-commandline'" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh public key file pub-commandline" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-commandline" $LOG_FILE

        LOG_FILE="ssh_commandline_default.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz >$LOG_FILE 2>&1"

        rlAsserNottGrep "Using SSH public key 'pub-commandline'" $LOG_FILE
        rlAssertNotGrep "Using SSH private key 'pri-commandline'" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh public key file pub-commandline" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh private key file pri-commandline" $LOG_FILE
    rlPhaseEnd

    rlPhaseStartTest "set SSH keyfile conf file upload.conf"
        # ssh keys are set in upload.conf file
        upload_conf="/etc/libreport/plugins/upload.conf"
        rlFileBackup $upload_conf

cat > $upload_conf <<EOF
SSHPublicKey = pub-conffile
SSHPrivateKey = pri-conffile
EOF

        LOG_FILE="ssh_conffile.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-conffile'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-conffile'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-conffile" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-conffile" $LOG_FILE

cat > $upload_conf <<EOF
SSHPublicKey = pub-conffile
#SSHPrivateKey = pri-conffile
EOF

        LOG_FILE="ssh_conffile_only_public.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-conffile'" $LOG_FILE
        rlAssertNotGrep "Using SSH private key 'pri-conffile'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-conffile" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh private key file pri-conffile" $LOG_FILE

cat > $upload_conf <<EOF
#SSHPublicKey = pub-conffile
SSHPrivateKey = pri-conffile
EOF

        LOG_FILE="ssh_conffile_only_private.log"
        rlRun "./expect reporter-upload -vvv -d problem_dir -u scp://root@localhost$TmpDir/target/upload.tar.gz >$LOG_FILE 2>&1"

        rlAssertNotGrep "Using SSH public key 'pub-conffile'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-conffile'" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh public key file pub-conffile" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-conffile" $LOG_FILE

        rlFileRestore
    rlPhaseEnd

    rlPhaseStartTest "set SSH keyfile by env var - conf file"
        # ssh keys are set in report_Uploader's events conf file
        event_upload_conf="/etc/libreport/events/report_Uploader.conf"
        rlFileBackup $event_upload_conf
        event_conf_file="/etc/libreport/events.d/test_uploader_event.conf"

cat > $event_conf_file <<EOF
EVENT=report_Uploader
    reporter-upload
EOF

cat > $event_upload_conf <<EOF
Upload_URL = scp://root@localhost$TmpDir/target/upload.tar.gz
Upload_SSHPublicKey = pub-env-conf
Upload_SSHPrivateKey = pri-env-conf
EOF
        LOG_FILE="ssh_env.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-env-conf'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-env-conf'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-env-conf" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-env-conf" $LOG_FILE

cat > $event_upload_conf <<EOF
Upload_URL = scp://root@localhost$TmpDir/target/upload.tar.gz
#Upload_SSHPublicKey = pub-env-conf
Upload_SSHPrivateKey = pri-env-conf
EOF

        LOG_FILE="ssh_env_only_private.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertNotGrep "Using SSH public key 'pub-env-conf'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-env-conf'" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh public key file pub-env-conf" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-env-conf" $LOG_FILE

cat > $event_upload_conf <<EOF
Upload_URL = scp://root@localhost$TmpDir/target/upload.tar.gz
Upload_SSHPublicKey = pub-env-conf
#Upload_SSHPrivateKey = pri-env-conf
EOF
        LOG_FILE="ssh_env_only_public.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-env-conf'" $LOG_FILE
        rlAssertNotGrep "Using SSH private key 'pri-env-conf'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-env-conf" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh private key file pri-env-conf" $LOG_FILE

        rm -r $event_upload_conf
        rlFileRestore
    rlPhaseEnd

    rlPhaseStartTest "set SSH keyfile by env var - conf file"
        # tests if reporter_Uploader events conf file export env variable properly
        event_upload_conf="/etc/libreport/events/report_Uploader.conf"
        rlFileBackup $event_upload_conf

cat > $event_upload_conf <<EOF
Upload_URL = scp://root@localhost$TmpDir/target/upload.tar.gz
EOF
        event_upload_xml="/etc/libreport/events/report_Uploader.xml"
        # do not use rlFileBackup because the file is restored many times
        rlRun "cp -fv $event_upload_xml ${event_upload_xml}.backup"

        sed -i -e "s/name=\"Upload_SSHPublicKey\">/name=\"Upload_SSHPublicKey\"><default-value>pub-env-xml<\/default-value>/g" $event_upload_xml
        sed -i -e "s/name=\"Upload_SSHPrivateKey\">/name=\"Upload_SSHPrivateKey\"><default-value>pri-env-xml<\/default-value>/g" $event_upload_xml

        LOG_FILE="ssh_env_xml.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-env-xml'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-env-xml'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-env-xml" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-env-xml" $LOG_FILE

        rlRun "cp -fv ${event_upload_xml}.backup $event_upload_xml"

        sed -i -e "s/name=\"Upload_SSHPublicKey\">/name=\"Upload_SSHPublicKey\"><default-value>pub-env-xml<\/default-value>/g" $event_upload_xml

        LOG_FILE="ssh_env_xml_only_public.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertGrep "Using SSH public key 'pub-env-xml'" $LOG_FILE
        rlAssertNotGrep "Using SSH private key 'pri-env-xml'" $LOG_FILE
        rlAssertGrep "curl: Using ssh public key file pub-env-xml" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh private key file pri-env-xml" $LOG_FILE

        rlRun "cp -fv ${event_upload_xml}.backup $event_upload_xml"

        sed -i -e "s/name=\"Upload_SSHPrivateKey\">/name=\"Upload_SSHPrivateKey\"><default-value>pri-env-xml<\/default-value>/g" $event_upload_xml

        LOG_FILE="ssh_env_xml_only_private.log"
        rlRun "./expect report-cli -vvv -r problem_dir >$LOG_FILE 2>&1"

        rlAssertNotGrep "Using SSH public key 'pub-env-xml'" $LOG_FILE
        rlAssertGrep "Using SSH private key 'pri-env-xml'" $LOG_FILE
        rlAssertNotGrep "curl: Using ssh public key file pub-env-xml" $LOG_FILE
        rlAssertGrep "curl: Using ssh private key file pri-env-xml" $LOG_FILE

        rlRun "mv -fv ${event_upload_xml}.backup $event_upload_xml"
        rm -r $event_upload_conf
        rlFileRestore
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "export EDITOR=$editor_var"
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
