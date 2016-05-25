#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of oops-sanity
#   Description: does sanity on abrt-dump-oops
#   Author: Matej Habrnal <mhabrnal@redhat.com
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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
. ../aux/lib.sh

TEST="oops-sanity"
PACKAGE="abrt"
OOPS_REQUIRED_FILES="kernel uuid duphash
pkg_name pkg_arch pkg_epoch pkg_release pkg_version"
EXAMPLES_PATH="../../examples"

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf
        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)

        cp $EXAMPLES_PATH/10_oopses.test $TmpDir
        cp $EXAMPLES_PATH/1_oops.test $TmpDir
        cp $EXAMPLES_PATH/no_oops.test $TmpDir

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "abrt-dump-oops -u"
        prepare

        installed_kernel="$( rpm -q kernel | tail -n1 )"
        kernel_version="$( rpm -q --qf "%{version}" $installed_kernel )"
        sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" 10_oopses.test
        sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" 1_oops.test

        mkdir crash_dir
        crash_PATH="$TmpDir/crash_dir"
        echo -n "1436876948" > $crash_PATH/time
        echo -n "Kerneloops" > $crash_PATH/type

        # 10 oopses
        rlAssertNotExists $crash_PATH/backtrace
        rlAssertNotExists $crash_PATH/kernel
        rlAssertNotExists $crash_PATH/reason

        rlRun "abrt-dump-oops -u $crash_PATH -vvv 10_oopses.test &> oops.log" 0

        rlAssertExists $crash_PATH/backtrace
        rlAssertExists $crash_PATH/kernel
        rlAssertExists $crash_PATH/reason

        rlAssertGrep "CPU: 0 PID: 37 Comm: kworker/0:1 Not tainted" $crash_PATH/backtrace
        rlAssertGrep $kernel_version $crash_PATH/kernel
        rlAssertGrep "WARNING: CPU: 0 PID: 37 at drivers/gpu/drm/radeon/radeon_gart.c:235" $crash_PATH/reason

        rlAssertGrep "Updating problem directory" oops.log
        rlAssertGrep "More oopses found: process only the first one" oops.log
        rlAssertNotGrep "Can't update the problem: no oops found" oops.log

        # 1 oops
        rm $crash_PATH/backtrace $crash_PATH/kernel $crash_PATH/reason
        rlAssertNotExists $crash_PATH/backtrace
        rlAssertNotExists $crash_PATH/kernel
        rlAssertNotExists $crash_PATH/reason

        rlRun "abrt-dump-oops -u $crash_PATH -vvv 1_oops.test &> one_oops.log" 0

        rlAssertExists $crash_PATH/backtrace
        rlAssertExists $crash_PATH/kernel
        rlAssertExists $crash_PATH/reason

        rlAssertGrep "CPU: 0 PID: 37 Comm: kworker/0:1 Not tainted" $crash_PATH/backtrace
        rlAssertGrep $kernel_version $crash_PATH/kernel
        rlAssertGrep "WARNING: CPU: 0 PID: 37 at drivers/gpu/drm/radeon/radeon_gart.c:235" $crash_PATH/reason

        rlAssertGrep "Updating problem directory" one_oops.log
        rlAssertNotGrep "More oopses found: process only the first one" one_oops.log
        rlAssertNotGrep "Can't update the problem: no oops found" one_oops.log

        # no oops
        rm $crash_PATH/backtrace $crash_PATH/kernel $crash_PATH/reason
        rlAssertNotExists $crash_PATH/backtrace
        rlAssertNotExists $crash_PATH/kernel
        rlAssertNotExists $crash_PATH/reason

        rlRun "abrt-dump-oops -u $crash_PATH -vvv no_oops.test &> no_oops.log" 1

        rlAssertNotExists $crash_PATH/backtrace
        rlAssertNotExists $crash_PATH/kernel
        rlAssertNotExists $crash_PATH/reason

        rlAssertGrep "Updating problem directory" no_oops.log
        rlAssertNotGrep "More oopses found: process only the first one" no_oops.log
        rlAssertGrep "Can't update the problem: no oops found" no_oops.log

        rlRun "rm -rf $crash_PATH" 0 "Remove crash directory"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(echo *log)
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
