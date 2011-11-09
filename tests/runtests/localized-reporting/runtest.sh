#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of localized-reporting
#   Description: tests localized reporting via report-cli
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

TEST="localized-reporting"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlLog "abrt-cli: $(abrt-cli list -f)"
        crash_PATH=$(abrt-cli list -f | grep Directory | tail -n1 | awk '{ print $2 }')
        if [ ! -d "$crash_PATH" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        rlLog "PATH = $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun 'echo -e "1\n1\n" \
            | EDITOR="cat" LC_ALL="cs_CZ.UTF-8" \
              abrt-cli report $crash_PATH 2>&1 \
            | tee crash.log \
            | grep "Architektura"' \
            0 "Localized string 'Architektura' present"
        #rlLog "$(cat crash.log)"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs "crash.log" crash.log
        popd
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd

