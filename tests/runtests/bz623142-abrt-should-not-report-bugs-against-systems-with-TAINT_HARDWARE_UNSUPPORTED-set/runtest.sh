#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz623142-abrt-should-not-report-bugs-against-systems-with-TAINT_HARDWARE_UNSUPPORTED-set
#   Description: abrt should not report bugs against systems with TAINT_HARDWARE_UNSUPPORTED set
#   Author: Michal Nowak <mnowak@redhat.com>
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

TEST="bz623142-abrt-should-not-report-bugs-against-systems-with-TAINT_HARDWARE_UNSUPPORTED-set"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        rlRun "tar xvf taint.tar -C $TmpDir" 0 "Extract file taint.tar"
        rlRun "pushd ${TmpDir}/taint"
        dump_ARCH=$ARCH
        unset ARCH
        rlRun "make all" 0 "Build taint.ko"
        ARCH=$dump_ARCH
        rlAssertExists "taint.ko"
        rlAssert0 "Kernel is not tained" $(cat /proc/sys/kernel/tainted)
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "insmod ./taint.ko" 0 "Loaded taint.ko"
        rlRun "lsmod | grep taint" 0 "taint.ko is loaded"
        rlAssertGreater "tainted is set to non-zero ($(cat /proc/sys/kernel/tainted))" $(cat /proc/sys/kernel/tainted) 0
        rlRun "rmmod taint" 0 "Loaded taint.ko"
        rlLogInfo "You should now restart this machine to ged rid of tained kernel"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
