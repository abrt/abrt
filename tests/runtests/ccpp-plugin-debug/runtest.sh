#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-debug
#   Description: Tests debugging functionality of ccpp-plugin
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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

TEST="ccpp-plugin-debug"
PACKAGE="abrt"
ABRT_CONF=/etc/abrt/abrt.conf
ABRT_BINARY_NAME="abrt_will_segfault"
SECRET_INFORMATION="Awesome, ABRT is!"


function assert_number_of_files
{
# $1 - directory
# $2 - expected number
# $3 - message

    if [ "_$2" != "_$(ls -l $1 | wc -l | cut -f1 -d' ')" ]; then
        # Simplify debugging
        ls -al $1

        rlFail $3
    fi
}

function prepare_test_case
{
# $1 -debug level

    rlLog "Set DebugLevel=$1"
    sed "/DebugLevel/d" -i $ABRT_CONF
    echo "DebugLevel=$1" >> $ABRT_CONF

    rlLog "Remove all files from $ABRT_CONF_DUMP_LOCATION"
    rm -rf $ABRT_CONF_DUMP_LOCATION/*
}

function assert_file_is_coredump
{
# $1 full file path
    rlAssertEquals "$1 is coredump" "_$1: application/x-coredump; charset=binary" "_$(file -i $1)"
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        load_abrt_conf

        ABRT_BINARY_COREDUMP=$ABRT_CONF_DUMP_LOCATION/$ABRT_BINARY_NAME"-coredump"

        TmpDir=$(mktemp -d)
        cp $(which will_segfault) $TmpDir/$ABRT_BINARY_NAME
        pushd $TmpDir

        rlFileBackup $ABRT_CONF
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=0 -> no core dump"
        # abrt-hook-ccpp should not do any action if it handles a crash of a
        # process executing a program whose name starts with 'abrt'.
        #
        # To be completely sure it does nothing, we remove all files from
        # the dump location and test if no new file is created.

        prepare_test_case 0

        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Generate crash"
        PID=$(./$ABRT_BINARY_NAME & echo $!)
        wait_for_process "abrt-hook-ccpp"

        # "total 1" + last-ccpp
        assert_number_of_files $ABRT_CONF_DUMP_LOCATION 2 "Crash of ABRT binary caused a new file in the dump location"

        UID=$(id -u)
        journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since="$SINCE" | tee no_debug.log
        rlAssertGrep "Process $PID ($ABRT_BINARY_NAME) of user $UID killed by SIGSEGV - ignoring ('DebugLevel' == 0)" no_debug.log
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=1 -> core dump created"
        prepare_test_case 1

        ./$ABRT_BINARY_NAME
        wait_for_process "abrt-hook-ccpp"

        rlAssertExists $ABRT_BINARY_COREDUMP
        assert_file_is_coredump $ABRT_BINARY_COREDUMP

        # "total 2" + last-ccpp + the core file
        assert_number_of_files $ABRT_CONF_DUMP_LOCATION 3 "Crash of ABRT binary caused too many new files"

        rm -rf $ABRT_BINARY_COREDUMP
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=1 -> does not overwrite files"
        prepare_test_case 1

        rlLog "Create a malicious file"
        echo "$SECRET_INFORMATION" > $ABRT_BINARY_COREDUMP

        # We need to verify that the hook uses O_EXCL (man 2 open) and we know
        # that the hook removes the file prior opening it, so we need to cause
        # the remove operation to fail.
        # We run under root so removing 'w' won't work.
        rlLog "Make the dump location immutable"
        chattr +i $ABRT_CONF_DUMP_LOCATION

        # get rid of logs from the last run
        sleep 1
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Generate crash"
        ./$ABRT_BINARY_NAME
        wait_for_process "abrt-hook-ccpp"

        rlLog "Remove -i from the dump location"
        chattr -i $ABRT_CONF_DUMP_LOCATION

        rlAssertExists $ABRT_BINARY_COREDUMP
        rlAssertEquals "The file was not overwritten" "_$SECRET_INFORMATION" "_$(cat $ABRT_BINARY_COREDUMP)"

        journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since="$SINCE" | tee no_overwrite.log
        rlAssertGrep "Can't open '$ABRT_BINARY_COREDUMP': File exists" no_overwrite.log

        # assert_number_of_files is useless because of 'chattr -i'

        rm -rf $ABRT_BINARY_COREDUMP
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=1 -> handles directory gracefully"
        prepare_test_case 1

        rlLog "Create a clumsy directory"
        mkdir -p $ABRT_BINARY_COREDUMP

        # get rid of logs from the last run
        sleep 1
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Generate crash"
        ./$ABRT_BINARY_NAME
        wait_for_process "abrt-hook-ccpp"

        if [ ! -d $ABRT_BINARY_COREDUMP ]; then
            rlFail "abrt-hook-ccpp removed a directory"
        fi

        journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since="$SINCE" | tee is_directory.log
        rlAssertGrep "Can't open '$ABRT_BINARY_COREDUMP': File exists" is_directory.log

        # "total 2" + last-ccpp + the core file
        assert_number_of_files $ABRT_CONF_DUMP_LOCATION 3 "Crash of ABRT binary caused too many new files"

        rm -rf $ABRT_BINARY_COREDUMP
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=1 -> does not overwrite hard links"
        prepare_test_case 1

        rlLog "Create a hard link"
        echo $SECRET_INFORMATION > $ABRT_CONF_DUMP_LOCATION/abrt_test_hardlink
        ln -f $ABRT_CONF_DUMP_LOCATION/abrt_test_hardlink $ABRT_BINARY_COREDUMP

        if [ ! -e $ABRT_BINARY_COREDUMP ]; then
            rlFail "Bug in the test: $ABRT_BINARY_COREDUMP hard link should exists"
        fi

        # get rid of logs from the last run
        sleep 1
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Generate crash"
        ./$ABRT_BINARY_NAME
        wait_for_process "abrt-hook-ccpp"

        rlAssertExists $ABRT_BINARY_COREDUMP
        assert_file_is_coredump $ABRT_BINARY_COREDUMP
        rlAssertEquals "The hard link was not overwritten" "_$SECRET_INFORMATION" "_$(cat $ABRT_CONF_DUMP_LOCATION/abrt_test_hardlink)"

        # "total 2" + last-ccpp + the core file + the hard link
        assert_number_of_files $ABRT_CONF_DUMP_LOCATION 4 "Crash of ABRT binary caused too many new files"

        rm -rf $ABRT_BINARY_COREDUMP
        rm -rf $ABRT_CONF_DUMP_LOCATION/abrt_test_hardlink
    rlPhaseEnd

    rlPhaseStartTest "DebugLevel=1 -> does not follow symbolic links"
        prepare_test_case 1

        rlLog "Create a malicious symbolic link"
        echo $SECRET_INFORMATION > /tmp/abrt_secret_file
        ln -sf /tmp/abrt_secret_file $ABRT_BINARY_COREDUMP

        rlLog "Generate crash"
        ./$ABRT_BINARY_NAME
        wait_for_process "abrt-hook-ccpp"

        rlAssertExists $ABRT_BINARY_COREDUMP
        assert_file_is_coredump $ABRT_BINARY_COREDUMP
        rlAssertEquals "the symlink isn't touched" "_$SECRET_INFORMATION" "_$(cat /tmp/abrt_secret_file)"

        # "total 2" + last-ccpp + the core file
        assert_number_of_files $ABRT_CONF_DUMP_LOCATION 3 "Crash of ABRT binary caused too many new files"

        rm -rf $ABRT_BINARY_COREDUMP
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
