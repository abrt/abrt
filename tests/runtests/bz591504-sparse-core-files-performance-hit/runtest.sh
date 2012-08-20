#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz591504-sparse-core-files-performance-hit
#   Description: test sparse core files performance hit
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

TEST="bz591504-sparse-core-files-performance-hit"
PACKAGE="abrt"
GDB_BUILDDEPS="expat-devel ncurses-devel rpm-devel bison python-devel gettext flex zlib-devel texinfo-text texinfo"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        rlRun "cc bigcore.c -o $TmpDir/bigcore" 0 "Compiling bigcore.c"
        pushd $TmpDir
        rlRun "ulimit -c unlimited"
    rlPhaseEnd

    rlPhaseStartTest
        # Making sure abrt is intercepting coredumps
        # (otherwise test will "pass" but we'd not test abrt, just the kernel)
        rlAssertGrep "abrt-hook-ccpp" /proc/sys/kernel/core_pattern

        rlLog "Generating core"
        rlRun "rm core* 2>/dev/null; sh -c './bigcore; exit 0' &>/dev/null"
        rlAssertExists core*
        apparent_coresize=$(du -B1 --apparent-size core* | sed 's/[ \t].*//')
        actual_coresize=$(du -B1 core* | sed 's/[ \t].*//')
        rlLog "Core sizes: apparent:$apparent_coresize actual:$actual_coresize"

        # In my experience here apparent size is almost 500 times bigger
        rlAssertGreater "Corefile is very sparse" $((apparent_coresize/50)) $actual_coresize

    rlPhaseEnd

    rlPhaseStartCleanup
        # clean the awfully big coredumps, it causes the test machine to run out of space :(
        rm /var/spool/abrt/*/coredump 2>/dev/null
        popd # $TmpDir
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
rlJournalPrintText
rlJournalEnd
