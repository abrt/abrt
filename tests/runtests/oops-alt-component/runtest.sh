#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of oops-alt-component
#   Description: test ABRT's ability to detect certain modules in kernel oops
#   Author: Jakub Filak <jfilak@redhat.com>
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

TEST="oops-alt-component"
PACKAGE="abrt"
EXAMPLES_PATH="../../../examples"

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf
        LANG=""
        export LANG
        check_prior_crashes

        rlRun "TmpDir=$(mktemp -d)"
        rlRun "cp $(ls $EXAMPLES_PATH/oops*-module*.test | tr '\n' ' ') $TmpDir"

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            prepare

            installed_kernel="$( rpm -q kernel | tail -n1 )"
            kernel_version="$( rpm -q --qf "%{version}" $installed_kernel )"
            sed -i "s/Not tainted .* #/Not tainted $installed_kernel #/g" $oops
            rlRun "abrt-dump-oops $oops -xD 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "[$oops] Found OOPS"

            wait_for_hooks
            get_crash_path

            rlAssertGrep "xorg-x11-drv-$(basename ${oops##oops*-module-} .test)" "$crash_PATH/component"
            rlAssertGrep "kernel-maint@redhat.com" "$crash_PATH/extra-cc"
            rlAssertGrep "kernel" "$crash_PATH/pkg_name"
            rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"

            rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
