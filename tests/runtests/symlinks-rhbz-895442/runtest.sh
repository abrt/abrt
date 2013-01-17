#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh for symlink handling
#   Description: tests symlink handling - abrt and libreport shouldn't follow symlinks!
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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

TEST="symlinks"
PACKAGE="abrt"

PROBLEMDIR=`pwd`"/problem_dir"

rlJournalStart
rlPhaseStartSetup
        TmpDir=$(mktemp -d)

        gcc `pkg-config libreport --cflags --libs` notlink.c -o "$TmpDir/"notlink
        gcc `pkg-config libreport --cflags --libs` link.c -o "$TmpDir/"link

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "./notlink $PROBLEMDIR" 0 "Reading regular file"
        rlRun "./link $PROBLEMDIR" 0 "Reading link"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
