#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of libreport-make-check
#   Description: Runs make check in libreport's sources
#   Author: Richard Marko <rmarko@redhat.com>
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

TEST="libreport-make-check"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlRun "git clone git://git.fedorahosted.org/libreport.git" 0 "Clone libreport.git"
        pushd libreport/
        short_rev=$(git rev-parse --short HEAD)
        rlLog "Git short rev: $short_rev"
        yum-builddep -y --nogpgcheck libreport
        rlRun "./autogen.sh" 0 "Autogen"
        rlRun "rpm --eval '%configure' | sh # ./configure" 0 "Configure"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "make check" 0 "Make check"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # libreport/
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
