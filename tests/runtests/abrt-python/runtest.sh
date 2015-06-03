#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-python
#   Description: Tests basic functionality of python problem api
#   Author: Richard Marko <rmarko@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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

TEST="abrt-python"
PACKAGE="abrt"


rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp *.py $TmpDir
        cp watch_expected $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest monitor
        python watch.py > watch_output &

        generate_crash
        get_crash_path
        wait_for_hooks

        # Give at leas 1s to D-Bus to deliver Crash signal to watch.py
        # It could be possible to configure dbus-monitor somehow but adding
        # sleep is much faster and reliable.
        sleep 1

        kill %1
        rlAssertNotDiffer watch_output watch_expected
        rlRun "echo $crash_PATH_RIGHTS | grep drwxr-x---" 0 "Crash directory has proper rights"
        rlRun "[ 'x$crash_PATH_USER' == 'xroot' ]" 0 "Crash directory owned by root"
        rlRun "[ 'x$crash_PATH_GROUP' == 'xabrt' ]" 0 "Crash directory group is abrt"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest list
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "python -c 'import problem; assert len(problem.list()) == 1'"

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartTest edit
        prepare
        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "python edit.py"
        # ^ should delete coredump and replace will_segfault with 31337 in cmdline

        rlAssertNotExists $crash_PATH/coredump
        rlAssertGrep "31337" $crash_PATH/cmdline

        rlRun "python delete.py"
        rlAssertNotExists $crash_PATH
    rlPhaseEnd

    rlPhaseStartTest create
        prepare
        rlRun "python create.py" 
        get_crash_path
        echo $crash_PATH
        wait_for_hooks

        rlAssertExists $crash_PATH/reason
        rlAssertGrep "runtime" $crash_PATH/analyzer

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd


    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
