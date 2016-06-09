#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of mailx-reporting
#   Description: Tests abrt-action-mailx
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

TEST="mailx-reporting"
PACKAGE="abrt"

SLEEP_TIME=10

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp mailx.conf mailx_without_subject.conf $TmpDir
        cp -R problem_dir $TmpDir
        cp formatting_files/* $TmpDir
        rlRun "pushd $TmpDir"

        rlFileBackup /etc/localtime
        rlRun "ln -sf /usr/share/zoneinfo/Europe/Prague /etc/localtime"
        export LANG=C

        generate_crash
        get_crash_path
        wait_for_hooks
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "reporter-mailx -v -d $crash_PATH -c mailx.conf" 0 "Report via mailx"
        sleep $SLEEP_TIME
        rlRun "mailx -H -u root | tail -n1 > mail.out"
        rlAssertGrep "abrt@" mail.out
        rlAssertGrep "abrt@" /var/spool/mail/root
        rlAssertGrep "\[abrt\] crash" mail.out
        rlLog "$(cat mail.out)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email default formatting - subject form mailx.conf"
        FORM_FILE="default"
        LOG_FILE="defaul.log"
        rlRun "reporter-mailx -D -d problem_dir -c mailx.conf &> $LOG_FILE" 0 "Report via mailx"
        sleep $SLEEP_TIME
        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email default formatting - default subject"
        FORM_FILE="default_env_subject"
        LOG_FILE="defaul_env_subject.log"

        # set environment variable
        export Mailx_Subject="[abrt] environment variable"

        rlRun "reporter-mailx -D -d problem_dir -c mailx_without_subject.conf &> $LOG_FILE" 0 "Report via mailx"
        sleep 10
        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        unset Mailx_Subject
        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email default formatting - default subject"
        FORM_FILE="default_default_subject"
        LOG_FILE="defaul_default_subject.log"
        rlRun "reporter-mailx -D -d problem_dir -c mailx_without_subject.conf &> $LOG_FILE" 0 "Report via mailx"
        sleep $SLEEP_TIME
        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email default formatting - send binary files - debug"
        FORM_FILE="default_binary"
        LOG_FILE="defaul_binary.log"

        # send binary data
        echo "SendBinaryData=yes" >> mailx_without_subject.conf

        rlRun "reporter-mailx -D -d problem_dir -c mailx_without_subject.conf &> $LOG_FILE" 0 "Report via mailx"
        sleep $SLEEP_TIME

        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email default formatting - send binary files"
        # delete all root's mail
        rlRun "echo 'delete *' | mailx -u root" 0 "Delete mail"

        rlRun "reporter-mailx -d problem_dir -c mailx_without_subject.conf" 0 "Report via mailx"
        sleep $SLEEP_TIME

        rlAssertGrep " filename=\"coredump\"" /var/spool/mail/root
        rlAssertGrep " filename=\"sosreport.tar.xz\"" /var/spool/mail/root

        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email formatting - not existing conf file"
        rlRun "reporter-mailx -v -d $crash_PATH -c mailx.conf -F not_exist_format.conf &> mailx_no_formatting_file.log" 1 "Report via mailx"
        sleep 5
        rlAssertGrep "Invalid format file: not_exist_format.conf" mailx_no_formatting_file.log

        rlLog "$(cat mailx_no_formatting_file.log)"
    rlPhaseEnd

    rlPhaseStartTest "Email formatting by conf file"
        FORM_FILE="mailx_format.conf"
        LOG_FILE="mailx_format_conf"
        rlRun "reporter-mailx -D -d problem_dir -c mailx.conf -F $FORM_FILE &> $LOG_FILE" 0 "Report via mailx"
        sleep $SLEEP_TIME
        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartTest "Email formatting - attach reason file"
        FORM_FILE="mailx_format_attach.conf"
        LOG_FILE="mailx_format_attach_conf"
        rlRun "reporter-mailx -D -d $crash_PATH -c mailx.conf -F $FORM_FILE &> $LOG_FILE" 0 "Report via mailx"
        sleep $SLEEP_TIME
        rlAssertNotDiffer $LOG_FILE ${FORM_FILE}.right

        rlLog "$(cat $LOG_FILE)"
        rlLog "$(diff $LOG_FILE ${FORM_FILE}.right)"
        rlRun "rm -rf problem_dir/reported_to"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        rlBundleLogs abrt 'mail.out' $(ls *.log)
        rlRun "popd"
        rlRun "echo 'delete *' | mailx -u root" 0 "Delete mail"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
