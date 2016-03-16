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

TEST="verify-that-report-edits"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlRun "dnf -y install expect" 0 "Install expect"
        TmpDir=$(mktemp -d)
        cp "./fakeditor.sh" $TmpDir
        cp "./expect" $TmpDir
        pushd $TmpDir
        rlLog "Generate crash"
        sleep 3m &
        sleep 1
        kill -SIGSEGV %1
        # Give post-create (if any) a bit of time to finish working:
        sleep 5
        rlLog "abrt-cli: $(abrt-cli list)"
        crash_PATH=$(abrt-cli list | grep Directory | tail -n1 | awk '{ print $2 }')
        if [ ! -d "$crash_PATH" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        rlLog "crash_PATH=$crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest
        # Make uploader send tarball not to /tmp, but to our temp dir,
        # with fixed file name.
        Upload_URL="file://$TmpDir/out.tar.gz"
        export Upload_URL
        # This is the fragile part.
        # Expect must navigate us through all possible dialogs abrt-cli
        # might throw at us. This doesn't work stable enough for me :(
        # FIXME.
        rlRun "./expect $crash_PATH &>out" 0 "Running abrt-cli via expect"
        rlLog "$(cat out)"
        # Check the results
        rlAssertGrep "The report has been updated" out
        rlAssertGrep "edited successfully" out
        rlAssertNotGrep "!! Timeout !!" out
        smart_quote="$(grep 'smart_quote=' fakeditor.sh | awk -F '"' '{ print $2 }')"
        rlLog "smart_quote=$smart_quote"
        #
        # We used to invoke Logger, but now the default config makes "abrt-cli report"
        # to show these choices:
        # 1) New Red Hat Support case
        # 2) Existing Red Hat Support case
        # 3) Save to tar archive
        #
        # ./expect script (tries to) invoke choice (3)
        #
        #rlAssertGrep "/tmp/abrt.log" out
        #rlAssertGrep "$smart_quote" "/tmp/abrt.log"
        rlAssertGrep "Successfully sent .* to $Upload_URL" out
        rlRun "tar xf $TmpDir/out.tar.gz -O comment | grep -qF '$smart_quote'" \
                0 "Comment is edited in saved tarball"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Delete $crash_PATH"
        popd # TmpDir
        # Comment next line out, and examine $TmpDir contents when debugging:
        rlRun "rm -rf $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
