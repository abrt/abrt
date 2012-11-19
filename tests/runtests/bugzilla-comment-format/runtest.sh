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

TEST="bugzilla-comment-format"

PROBLEMDIR=`pwd`"/problem_dir"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp bugzilla*.conf output.expected $TmpDir
        rlRun "pushd $TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "BZ comment generation"
        rlRun "reporter-bugzilla -c bugzilla.conf -F bugzilla_format.conf -d $PROBLEMDIR --debug >output" 0 "Generating BZ comment"
        rlRun "diff -u output.expected output" 0 "Comparing with expected output"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
