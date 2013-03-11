#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ureport
#   Description: Verify that valid ureport is generated from dump directory contents
#   Author: Martin Milata <mmilata@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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

TEST="ureport"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp fakefaf.py ureport-valid.py $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "ccpp"
        generate_crash
        get_crash_path
        wait_for_hooks

        ./fakefaf.py ccpp_ureport &
        sleep 1
        rlRun "reporter-ureport -v --insecure --url http://localhost:12345/faf -d $crash_PATH &> ccpp_reporter" 0 "Send uReport"
        kill %1

        rlRun "./ureport-valid.py ccpp_ureport" 0 "Validate uReport contents"
        rlAssertGrep "THANKYOU" ccpp_reporter

        cp $crash_PATH/reported_to ccpp_reported_to
        rlAssertGrep "BTHASH=" ccpp_reported_to
        rlAssertGrep "retrace.fedoraproject.org/faf/reports" ccpp_reported_to
        rlAssertGrep "bugzilla.redhat.com/show_bug.cgi" ccpp_reported_to

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "python"
        generate_python_exception
        get_crash_path
        wait_for_hooks

        ./fakefaf.py python_ureport &
        sleep 1
        rlRun "reporter-ureport -v --insecure --url http://localhost:12345/faf -d $crash_PATH &> python_reporter" 0 "Send uReport"
        kill %1

        rlRun "./ureport-valid.py python_ureport" 0 "Validate uReport contents"
        rlAssertGrep "THANKYOU" python_reporter

        cp $crash_PATH/reported_to python_reported_to
        rlAssertGrep "BTHASH=" python_reported_to
        rlAssertGrep "retrace.fedoraproject.org/faf/reports" python_reported_to
        rlAssertGrep "bugzilla.redhat.com/show_bug.cgi" python_reported_to

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt ccpp_* python_*
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
