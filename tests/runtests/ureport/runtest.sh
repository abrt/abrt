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
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ./fakefaf.py ccpp_ureport &
        sleep 1
        # 70 is EXIT_STOP_EVENT_RUN
        rlRun "reporter-ureport -vvv --insecure --url http://localhost:12345/faf -d $crash_PATH &> ccpp_reporter" 70 "Send uReport"
        kill %1

        rlRun "./ureport-valid.py ccpp_ureport" 70 "Validate uReport contents"

        cp $crash_PATH/reported_to ccpp_reported_to
        rlAssertGrep "BTHASH=" ccpp_reported_to
        rlAssertGrep "retrace.fedoraproject.org/faf/reports" ccpp_reported_to
        rlAssertGrep "bugzilla.redhat.com/show_bug.cgi" ccpp_reported_to

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "python3"
        prepare
        generate_python3_exception
        wait_for_hooks
        get_crash_path

        ./fakefaf.py python3_ureport &
        sleep 1
        rlRun "reporter-ureport -vvv --insecure --url http://localhost:12345/faf -d $crash_PATH &> python3_reporter" 70 "Send uReport"
        kill %1

        rlRun "./ureport-valid.py python3_ureport" 70 "Validate uReport contents"

        cp $crash_PATH/reported_to python3_reported_to
        rlAssertGrep "BTHASH=" python3_reported_to
        rlAssertGrep "retrace.fedoraproject.org/faf/reports" python3_reported_to
        rlAssertGrep "bugzilla.redhat.com/show_bug.cgi" python3_reported_to

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "ureport with Authentication data"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path


        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/IncludeAuthData yes"
        rlRun "augtool set /files/etc/libreport/plugins/ureport.conf/AuthDataItems hostname"

        hostname="$(cat $crash_PATH/hostname)"

        ./fakefaf.py &
        sleep 1
        rlRun "reporter-ureport -vvv --insecure --url http://localhost:12345/faf -d $crash_PATH &> reporter-ureport.log" 70 "Send uReport"
        kill %1

        rlAssertGrep "\"auth\": {   \"hostname\": \"$hostname\"" reporter-ureport.log

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt ccpp_* python3_*
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
