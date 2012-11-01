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

    rlPhaseStartTest "avc reporting"
        rlRun "reporter-bugzilla -d $AVCPROBLEMDIR -c bugzilla.conf &> output2" 0 "Reporting already reported AVC"
        # Tired of tracking ever-changing rhbz#s: it was bug 755535,
        # then it was 871696, then 871790...
        #out="`cat output2 | tr -d '\n'`"; test "$REPORTING_OUTPUT" = "${out:0:118}";
        #rlRun "test \"$?\" = \"0\"" 0 "Comparing new output with expected output"
        rlAssertGrep "Bug is already reported:" output
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $AVCPROBLEMDIR/reported_to" 0 "Removing reported_to"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
