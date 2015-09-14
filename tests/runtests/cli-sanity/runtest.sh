#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of cli-sanity
#   Description: does sanity on report-cli
#   Author: Michal Nowak <mnowak@redhat.com>
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

TEST="cli-sanity"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup

        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "--version"
        rlRun "abrt-cli --version | grep 'abrt-cli'"
#        rlAssertEquals "abrt-cli and abrt-cli RPM claim the same version" "$(abrt-cli -V | awk '{ print $2 }')" "$(rpmquery --qf='%{VERSION}' abrt-cli)"
    rlPhaseEnd

    rlPhaseStartTest "--help"
        rlRun "abrt-cli --help" 0
        rlRun "abrt-cli --help 2>&1 | grep 'Usage: abrt-cli'"
    rlPhaseEnd


    rlPhaseStartTest "list"
        prepare

        generate_crash
        get_crash_path
        wait_for_hooks

        rlRun "abrt-cli list | grep -i 'executable'"
        rlRun "abrt-cli list | grep -i 'package'"
    rlPhaseEnd

    rlPhaseStartTest "list -n" # not reported only
        rlRun "abrt-cli list -n | grep -i 'executable'"
        rlRun "abrt-cli list -n | grep -i 'package'"
    rlPhaseEnd

    rlPhaseStartTest "report FAKEDIR"
        rlRun "abrt-cli report FAKEDIR" 1
    rlPhaseEnd

    rlPhaseStartTest "report DIR"
        DIR=$(abrt-cli list -n | grep 'Directory' | head -n1 | awk '{ print $2 }')
        echo -e "1\n" |  EDITOR="cat" abrt-cli report $DIR > output.out 2>&1

        rlAssertGrep "\-cmdline" output.out
        rlAssertGrep "\-kernel" output.out
    rlPhaseEnd

    rlPhaseStartTest "info DIR"
        DIR=$(abrt-cli list | grep 'Directory' | head -n1 | awk '{ print $2 }')
        rlRun "abrt-cli info $DIR"
        rlRun "abrt-cli info -d $DIR > info.out"
    rlPhaseEnd

    rlPhaseStartTest "list (after reporting)"
        DIR=$(abrt-cli list | grep 'Directory' | head -n1 | awk '{ print $2 }')

        # this should ensure that ABRT will consider the problem as reported
        rlRun "reporter-print -r -d $DIR -o /dev/null &> print.log"

        if ! cat $DIR/reported_to | grep -s "URL=file:///"; then
            rlLog "Adding an artifical URL to 'reported_to' file"
            rlAssertGrep "The report was stored to /dev/null" print.log
            echo "file: URL=file:///dev/null" > $DIR/reported_to
        fi

        rlRun "abrt-cli list | grep -i 'executable'"
        rlRun "abrt-cli list | grep -i 'package'"

        # this expects that reporter-print works and adds an URL to
        # the output file to the problem's data
        rlRun "abrt-cli list | grep -i 'file:///dev/null'"
    rlPhaseEnd

    rlPhaseStartTest "list -n (after reporting)" # list not-reported
        BYTESNUM=$(abrt-cli list -n | wc -c)
        rlAssert0 "No not-reported problem" "$BYTESNUM"
    rlPhaseEnd

    rlPhaseStartTest "info FAKEDIR"
        rlRun "abrt-cli info FAKEDIR" 1
        rlRun "abrt-cli info -d FAKEDIR" 1
    rlPhaseEnd

    rlPhaseStartTest "rm FAKEDIR"
        rlRun "abrt-cli rm FAKEDIR" 1
    rlPhaseEnd

    rlPhaseStartTest "rm DIR"
        DIR_DELETE=$(abrt-cli list | grep 'Directory' | head -n1 | awk '{ print $2 }')
        rlRun "abrt-cli rm $DIR_DELETE"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd

