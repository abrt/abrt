#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ccpp-plugin-core-size
#   Description: Test ABRT's ability to limit core file size.
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2016 Red Hat, Inc. All rights reserved.
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

TEST="ccpp-plugin-core-size"
PACKAGE="abrt"
CRASHER="bigcore"
ABRT_CONF=/etc/abrt/abrt.conf
CCPP_CONF=/etc/abrt/plugins/CCpp.conf
AASPD_CONF=/etc/abrt/abrt-action-save-package-data.conf

function assert_file_size_min
{
    rlAssertGreaterOrEqual "Minimal size of $1" $(stat -c "%s" $1) $2
}

function assert_file_size_max
{
    rlAssertGreaterOrEqual "Maximal size of $1" $2 $(stat -c "%s" $1)
}

function run_test
{
        rlRun "sed 's/#\? *\(MaxCrashReportsSize\).*$/\1 = '${ABRTD_LIMIT_MiB}'/' -i ${ABRT_CONF}"
        rlRun "sed 's/#\? *\(MaxCoreFileSize\).*$/\1 = '${CCPP_LIMIT_MiB}'/' -i ${CCPP_CONF}"
        rlRun "sed 's/#\? *\(MakeCompatCore\).*$/\1 = yes/' -i ${CCPP_CONF}"
        rlRun "ulimit -c ${USER_LIMIT_kiB}"

        prepare
        rlRun "rm -f core*"

        rlRun "./$CRASHER -M ${ALLOC_SIZE_MiB}" 134
        wait_for_hooks
        get_crash_path

        rlAssertExists ${crash_PATH}/coredump
        if [ $ABRT_REAL_LIMIT_MiB -eq 0 ]; then
            assert_file_size_min ${crash_PATH}/coredump $((ABRT_REAL_LIMIT_MiB * 1024 * 1024))
        else
            assert_file_size_max ${crash_PATH}/coredump $((ABRT_REAL_LIMIT_MiB * 1024 * 1024))
        fi

        rlAssertExists ${crash_PATH}/pid
        pid=$(cat ${crash_PATH}/pid)

        if [ "0" == "${USER_LIMIT_kiB}" ]; then
            rlAssertEquals "" "$(ls core*)" "No user core-files"
        else
            rlAssertExists core.${pid}
            if [ "unlimited" == "${USER_LIMIT_kiB}" ]; then
                assert_file_size_min core.${pid} $((ALLOC_SIZE_MiB * 1024 * 1024))
            else
                assert_file_size_max core.${pid} $((USER_LIMIT_kiB * 1024))
            fi
        fi

        rlRun "abrt-cli rm $crash_PATH"
}

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        load_abrt_conf

        TmpDir=$(mktemp -d)
        rlRun "gcc -Wall -std=gnu99 -pedantic -o $TmpDir/$CRASHER $CRASHER.c"
        pushd $TmpDir

        rlFileBackup $ABRT_CONF $CCPP_CONF $AASPD_CONF
        rlRun "sed 's/#\? *\(ProcessUnpackaged\).*$/\1 = yes/' -i ${AASPD_CONF}"
    rlPhaseEnd

    rlPhaseStartTest "All unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=0
        CCPP_LIMIT_MiB=0
        ABRT_REAL_LIMIT_MiB=0

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRTD limit - CCpp unlimited - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=128
        CCPP_LIMIT_MiB=0
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRTD unlimited - CCpp limit - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=0
        CCPP_LIMIT_MiB=128
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRTD greater than CCpp - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=256
        CCPP_LIMIT_MiB=128
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRTD lower than CCpp - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=128
        CCPP_LIMIT_MiB=256
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRTD equal to CCpp - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=128
        CCPP_LIMIT_MiB=128
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRT greater than allocated - User unlimited"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=1024
        CCPP_LIMIT_MiB=1024
        ABRT_REAL_LIMIT_MiB=0

        USER_LIMIT_kiB="unlimited"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRT greater than User"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=0
        CCPP_LIMIT_MiB=0
        ABRT_REAL_LIMIT_MiB=0

        USER_LIMIT_kiB="$((128 * 1024))"

        run_test
    rlPhaseEnd

    rlPhaseStartTest "ABRT equal User"
        ALLOC_SIZE_MiB=256

        ABRTD_LIMIT_MiB=128
        CCPP_LIMIT_MiB=128
        ABRT_REAL_LIMIT_MiB=128

        USER_LIMIT_kiB="$((128 * 1024))"

        run_test
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
