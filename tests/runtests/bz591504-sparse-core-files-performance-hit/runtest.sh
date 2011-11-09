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

rlJournalStart
    rlPhaseStartSetup
        rlRun "yum-builddep -y gdb" 0 "Build dependencies for gdb"
        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlRun "yumdownloader --source gdb" 0 "Download gdb sources"
        rlRun "yum-builddep -y gdb-*.src.rpm" 0 "Fetch gdb dependencies"
        rlRun "rpm -ivh gdb-*.src.rpm" 0 "Install gdb sources"
        specfile="$(rpm --eval '%_specdir')/gdb.spec"
        rlRun "rpmbuild -bp $specfile" 0 "Unpack and patch"
        gdbtestdir="$(rpm --eval '%_builddir')/gdb-*/gdb/testsuite"
        pushd $gdbtestdir
        rpm --eval '%configure' | sh # ./configure
        rlRun "make site.exp" 0 "Make gdb tests"
    rlPhaseEnd

    rlPhaseStartTest
        times=3      # number of repetitions
        stoptime=0   # runtime when abrt is stopped
        goldratio=5  # $stoptime * $goldratio:
                     # |  run with abrt on should not take more that five times
                     # |  compared to run with abrt turned off

        rlRun "killall abrtd" 0 "Killing abrtd"

        rlLog "Calculate run times with abrt turned off"
        rlRun "runtest gdb.base/bigcore.exp &> /dev/null" 0 "Pre-test run"
        for run in $(seq $times); do
            rlRun "/usr/bin/time -f '%e' -o bigcore-stop.time.$run runtest gdb.base/bigcore.exp"
        done
        for cnt in $(seq $times); do
            stoptime=$(echo "$(cat bigcore-stop.time.$cnt) + $stoptime" | bc -l)
        done
        stoptime=$(echo "$stoptime / $times" | bc)
        rlLog "Computed time with abrt stopped: $stoptime"

        rlRun "/usr/sbin/abrtd -s" 0 "Starting abrtd"

        starttimeout=$(echo "$goldratio * $stoptime" | bc)
        rlLog "Computed timeout: $starttimeout"

        rlRun "runtest gdb.base/bigcore.exp &> /dev/null" 0 "Pre-test run"
        for run in $(seq $times); do
            rlWatchdog "runtest gdb.base/bigcore.exp" $starttimeout
            ec=$?
            rlAssert0 "Test performed fine" $ec
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # $gdbtestdir
        popd # $TmpDir
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
rlJournalPrintText
rlJournalEnd
