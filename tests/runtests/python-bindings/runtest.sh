#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of python-bindings
#   Description: Tests the python-bindings for ABRT API
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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

# Include rhts environment
. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="python-bindings"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp notify_new_path.py $TmpDir
        pushd $TmpDir
        CRASHDIR=$(cat /etc/abrt/abrt.conf | grep DumpLocation | sed 's/.* = //')
    rlPhaseEnd

    rlPhaseStartTest
        newpath="/this/is/the/testing/problem/path"
        rlRun "./notify_new_path.py $newpath" 0 "Testing notification over socket"
        sleep 1  # give abrt-server a while to print the message to syslog
        rlAssertGrep "Bad problem directory name '$newpath', should start with: '$CRASHDIR'" /var/log/messages
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # abrt/
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
