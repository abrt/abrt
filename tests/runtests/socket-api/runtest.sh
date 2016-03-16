#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrtd socket API
#   Description: tests if socket API works correctly
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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

TEST="socket-api"
PACKAGE="abrt"

TEST_APP="create_problem_socket"
TEST_APP_SRC=$TEST_APP".c"

CFG_FILE="/etc/abrt/abrt-action-save-package-data.conf"

rlJournalStart
rlPhaseStartSetup
        check_prior_crashes

        rlFileBackup $CFG_FILE
        sed -i 's/ProcessUnpackaged = no/ProcessUnpackaged = yes/g' $CFG_FILE

        rlRun "dnf install -y abrt-devel libreport-devel" 0 "installed required devel packages"

        TmpDir=$(mktemp -d)
        cp $TEST_APP_SRC $TmpDir
        pushd $TmpDir
        rlRun "gcc `pkg-config abrt --libs --cflags` $TEST_APP_SRC -o $TEST_APP" 0 "Testing app compiled successfully"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "./$TEST_APP" 0 "Response from server: success"

        get_crash_path
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        popd #TmpDir
        rm -rf $TmpDir
        rlFileRestore # CFG_FILE
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
