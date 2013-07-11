#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of cli-status
#   Description: does sanity on abrt-cli
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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

TEST="cli-status"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "status --help"
        rlRun "abrt-cli status --help" 0
        rlRun "abrt-cli status --help 2>&1 | grep 'Usage: abrt-cli'"
        rlRun "abrt-cli status --help 2>&1 | grep 'bare'"
        rlRun "abrt-cli status --help 2>&1 | grep 'since'"
    rlPhaseEnd

    rlPhaseStartTest "status"
        rlRun "abrt-cli status" 0
        rlRun "abrt-cli status --bare" 0
        rlRun "abrt-cli status --since 0" 0

        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "abrt-cli status | grep \"ABRT has detected 1 problem(s).\"" 0

        sleep 2 #just to make sure that SINCE > time of the previous crash
        SINCE=`date +%s`

        generate_python_segfault
        get_crash_path
        wait_for_hooks

        rlRun "abrt-cli status | grep \"ABRT has detected 2 problem(s).\"" 0

        rlRun "abrt-cli status --since=$SINCE | grep \"ABRT has detected 1 problem(s).\"" 0

    rlPhaseEnd



    rlPhaseStartCleanup
        find $( dirname $crash_PATH ) -mindepth 1 -type d | xargs rm -rf
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd

