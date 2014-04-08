#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz783450-setuid-core-owned-by-root
#   Description: Tests ccpp-plugin on suided app
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

TEST="bz783450-setuid-core-owned-by-root"
PACKAGE="abrt"
SUIDEDEXE="suidedexecutable"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        chmod a+rwx $TmpDir
        cp loop.c $TmpDir
        pushd $TmpDir
        rlRun "useradd abrt-suid-test -M" 0
        rlRun "echo \"kokotice\" | passwd abrt-suid-test --stdin"

    rlPhaseEnd

    rlPhaseStartTest "not setuid dump"
        rlLog "ulimit: `ulimit -c`"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlRun "kill -SIGSEGV $killpid"
        c=0;while [ ! -f "core.$killpid" ]; do sleep 0.1; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlLog "core: `ls -l`"
        rlRun '[ "xabrt-suid-test" == "x$(ls -l | grep "core.$killpid" | cut -d" " -f3)" ]' 0 "Checking if core is owned by abrt-suid-test"
    rlPhaseEnd

    rlPhaseStartTest "secure setuid dump"
        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 2 > /proc/sys/fs/suid_dumpable" 0 "Set setuid secure dump"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        rlRun "chmod u+s $SUIDEDEXE"
        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlRun "kill -SIGSEGV $killpid"
        c=0;while [ ! -f "core.$killpid" ]; do sleep 0.1; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlLog "core: `ls -l`"
        rlRun '[ "xroot" == "x$(ls -l | grep "core.$killpid" | cut -d" " -f3)" ]' 0 "Checking if core is owned by root"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "ulimit -c $old_ulimit" 0
        rlRun "userdel -r -f abrt-suid-test" 0

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
