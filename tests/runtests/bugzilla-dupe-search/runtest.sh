#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bugzilla-dupe-search
#   Description: Tests reporter-bugzilla duplicate search
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

TEST="bugzilla-dupe-search"
PACKAGE="abrt"
DUPHASH="5d25425e3589df3280ac8331554b2b1fa8384e49"
REPORTING_OUTPUT="Logging into Bugzilla at https://bugzilla.redhat.com/Checking for duplicatesBug is already reported: 871696Logging out"

AVCPROBLEMDIR=`pwd`"/avc-problem-dir"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp bugzilla.conf $TmpDir
        rlRun "pushd $TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "simple dupehash search"
        rlRun "reporter-bugzilla --duphash=$DUPHASH -c bugzilla.conf > output" 0 "Finding dupe for dupehash: $DUPHASH"
        rlAssertGrep "768647" output
    rlPhaseEnd

    rlPhaseStartTest "simple dupehash search with parameter -p"
        rlLog "Parameter -pProduct_parameter"
        rlRun "reporter-bugzilla -vvv --duphash=$DUPHASH -c bugzilla.conf -pProduct_parameter &> parameter_p.log" 0 "Finding dupe for dupehash: $DUPHASH"

        rlAssertGrep "Using Bugzilla product 'Product_parameter' to find duplicate bug" parameter_p.log
        rlAssertGrep "search for 'ALL whiteboard:\"abrt_hash:5d25425e3589df3280ac8331554b2b1fa8384e49\" product:\"Product_parameter\"'" parameter_p.log

        rlLog "Only parameter -p (get product from /etc/os-release)"
        rlRun "reporter-bugzilla -vvv --duphash=$DUPHASH -c bugzilla.conf -p &> only_parameter_p.log" 0 "Finding dupe for dupehash: $DUPHASH"

        rlRun "source /etc/os-release"
        rlAssertGrep "os-release:.*: parsed line: 'REDHAT_BUGZILLA_PRODUCT'" only_parameter_p.log -E
        rlAssertGrep "Using Bugzilla product '$REDHAT_BUGZILLA_PRODUCT' to find duplicate bug" only_parameter_p.log -E
        rlAssertGrep "search for 'ALL whiteboard:\"abrt_hash:5d25425e3589df3280ac8331554b2b1fa8384e49\" product:\".*\"'" only_parameter_p.log -E

        rlLog "Only parameter -p (get product from environment variable)"
        rlRun "export Bugzilla_Product='Product_env'" 0
        rlRun "reporter-bugzilla -vvv --duphash=$DUPHASH -c bugzilla.conf -p &> only_parameter_p_env.log" 0 "Finding dupe for dupehash: $DUPHASH"

        rlAssertGrep "Using Bugzilla product 'Product_env' to find duplicate bug" only_parameter_p_env.log -E
        rlAssertGrep "search for 'ALL whiteboard:\"abrt_hash:5d25425e3589df3280ac8331554b2b1fa8384e49\" product:\"Product_env\"'" only_parameter_p_env.log -E
        rlRun "unset Bugzilla_Product" 0
    rlPhaseEnd

    rlPhaseStartTest "avc reporting"
        rlRun "reporter-bugzilla -d $AVCPROBLEMDIR -c bugzilla.conf &> output" 0 "Reporting already reported AVC"
        # Tired of tracking ever-changing rhbz#s: it was bug 755535,
        # then it was 871696, then 871790...
        #out="`cat output2 | tr -d '\n'`"; test "$REPORTING_OUTPUT" = "${out:0:118}";
        #rlRun "test \"$?\" = \"0\"" 0 "Comparing new output with expected output"
        rlAssertGrep "Bug is already reported:" output
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt output $(ls *.log)
        rlRun "popd"
        rlRun "rm -r $AVCPROBLEMDIR/reported_to" 0 "Removing reported_to"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
