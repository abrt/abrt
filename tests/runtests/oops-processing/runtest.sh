#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of oops_processing
#   Description: test for required files in dump directory of koops
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

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="oops_processing"
PACKAGE="abrt"
OOPS_REQUIRED_FILES="kernel core_backtrace kernel
pkg_name pkg_arch pkg_epoch pkg_release pkg_version"
EXAMPLES_PATH="../../../examples"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops1.test > \
            $TmpDir/oops1.test

        sed "s/3.0.0-1.fc16.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops5.test > \
            $TmpDir/oops5.test

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            installed_kernel="$( rpm -q kernel | tail -n1 )"
            kernel_version="$( rpm -q --qf "%{version}" $installed_kernel )"
            sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" $oops
            rlRun "abrt-dump-oops $oops -xD 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "[$oops] Found OOPS"

            sleep 5
            rlAssertGreater "Crash recorded" $(abrt-cli list | wc -l) 0
            crash_PATH="$(abrt-cli list -f | grep Directory | awk '{ print $2 }' | tail -n1)"
            if [ ! -d "$crash_PATH" ]; then
                rlDie "No crash dir generated, this shouldn't happen"
            fi

            ls $crash_PATH > crash_dir_ls

            for f in $OOPS_REQUIRED_FILES; do
                rlAssertExists "$crash_PATH/$f"
            done

            rlAssertGrep "kernel" "$crash_PATH/pkg_name"
            rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"

            rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(echo *_ls)
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
