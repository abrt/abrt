#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-make-check
#   Description: Clone latest abrt, build and run make check
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

# Include rhts environment
. /usr/share/beakerlib/beakerlib.sh

TEST="abrt-make-check"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlRun "yum install -y git yum-utils rpm-build libtool intltool btparser-devel make"

        yum-builddep -y --nogpgcheck abrt
        rpmquery btparser-devel > /dev/null 2>&1 || yum install -y btparser-devel --enablerepo="updates-testing"
        rlRun "git clone git://git.fedorahosted.org/abrt.git" 0 "Clone abrt.git"
        pushd abrt/
        rlRun "./autogen.sh" 0 "Autogen"
        rlRun "rpm --eval '%configure' | sh" 0 "Configure"
        rlRun "make" 0 "Build"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "make check" 0 "Make check"
        if [ -e tests/testsuite.log ]; then
            cp tests/testsuite.log $ABRT_TESTOUT_ROOT/${TEST}-testsuite.log
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # abrt/
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
