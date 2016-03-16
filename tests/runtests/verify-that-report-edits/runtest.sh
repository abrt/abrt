#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of verify-that-report-edits
#   Description: Make sure manual edits to reports actually make it into submitted data.
#   Author: Michal Nowak <mnowak@redhat.com>, Richard Marko <rmarko@redhat.com>
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

TEST="verify-that-report-edits"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        cp test_event_*.conf /etc/libreport/events.d || exit $?
        cp test_event_*.xml  /usr/share/libreport/events || exit $?

        TmpDir=$(mktemp -d)
#        rlRun "dnf -y install expect" 0 "Install expect"
        cp "./fakeditor.sh" $TmpDir
        cp "./expect" $TmpDir
        pushd $TmpDir

        generate_crash
#        wait_for_hooks
        get_crash_path
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "./expect $crash_PATH &> out" 0 "Running abrt-cli via expect"
        rlLog "$(cat out)"
        rlAssertGrep "The report has been updated" out

        smart_quote="$(grep 'smart_quote=' fakeditor.sh | awk -F '"' '{ print $2 }')"
        rlAssertGrep "edited successfully" out
        # Disabled in ./expect so far:
        #rlAssertGrep "/tmp/abrt.log" out
        rlAssertNotGrep "!! Timeout !!" out
        rlLog "smart_quote=$smart_quote"
        rlAssertGrep "$smart_quote" "$crash_PATH/comment"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Delete $crash_PATH"
        popd # TmpDir
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
        rlRun "rm /etc/libreport/events.d/test_event_*.conf" 0 "Removing test event config"
        rlRun "rm /usr/share/libreport/events/test_event_*.xml" 0 "Removing test event config"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
