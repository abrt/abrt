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
OOPS_REQUIRED_FILES="kernel uuid duphash
pkg_name pkg_arch pkg_epoch pkg_release pkg_version pkg_vendor pkg_fingerprint"
EXAMPLES_PATH="../../../examples"
# abrt should store "xorg-x11-drv-ati" in component file in those test files
XORG_TEST_FILES="oops1.test oops_not_reportable_no_reliable_frame.test"

function is_xorg_test_file() {
    for xtf in $XORG_TEST_FILES; do
        if [ $xtf == $1 ]; then
            return 0;
        fi
    done

    return 1;
}

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf
        LANG=""
        export LANG
        check_prior_crashes

        TmpDir=$(mktemp -d)
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops1.test > \
            $TmpDir/oops1.test

        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_no_reliable_frame.test > \
            $TmpDir/oops_not_reportable_no_reliable_frame.test

        sed "s/3.0.0-1.fc16.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops5.test > \
            $TmpDir/oops5.test

        sed "s/3.10.0-33.el7.ppc64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops8_ppc64.test > \
            $TmpDir/oops8_ppc64.test

        sed "s/3.69.69-69.0.fit.s390x/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops10_s390x.test > \
            $TmpDir/oops10_s390x.test

        sed "s/3.10.0-41.el7.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_unsupported_hw.test > \
            $TmpDir/oops_not_reportable_unsupported_hw.test

        sed "s/2.6.35.6-45.fc14.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_broken_bios.test > \
            $TmpDir/oops_not_reportable_broken_bios.test

        sed "s/4.2.8-yocto-standard-1968fcd27440e21d39a0b9e14a3be2ad/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops-kernel-panic-hung-tasks-arm.test > \
            $TmpDir/oops_kernel_panic_hung_tasks_arm.test

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            prepare

            rpm_version="$( rpm -q --qf "%{version}-%{release}.%{arch}\n" kernel | tail -n 1 | tr -d '\n' )"
            kernel_epoch="$( rpm -q --qf "%|epoch?{%{epoch}}:{0}|\n" kernel | tail -n 1 | tr -d '\n' )"
            kernel_version="$( rpm -q --qf "%{version}\n" kernel | tail -n 1 | tr -d '\n' )"
            kernel_release="$( rpm -q --qf "%{release}\n" kernel | tail -n 1 | tr -d '\n' )"
            kernel_arch="$( rpm -q --qf "%{arch}\n" kernel | tail -n 1 | tr -d '\n' )"

            sed -i "s/<KERNEL_VERSION>/$rpm_version/g" $oops
            rlRun "abrt-dump-oops $oops -xD 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "[$oops] Found OOPS"

            wait_for_hooks
            get_crash_path

            ls $crash_PATH > crash_dir_ls

            for f in $OOPS_REQUIRED_FILES; do
                rlAssertExists "$crash_PATH/$f"
            done

            check_dump_dir_attributes $crash_PATH

            if [[ "$oops" == *not_reportable* ]]; then
                rlAssertExists "$crash_PATH/not-reportable"
            else
                if [ -f "$crash_PATH/not-reportable" ]; then
                    rlFail "$crash_PATH/not-reportable should not exist"
                    cat $crash_PATH/not-reportable
                fi
            fi

            rlAssertGrep "kernel" "$crash_PATH/pkg_name"

            is_xorg_test_file $oops
            if [ $? -eq 0 ]; then
                rlAssertGrep "xorg-x11-drv-ati" "$crash_PATH/component"
            else
                rlAssertGrep "kernel" "$crash_PATH/component"
            fi

            rlAssertEquals "Parsed kernel version" "$rpm_version" "$(cat $crash_PATH/kernel)"
            rlAssertEquals "kernel package epoch" "$kernel_epoch" "$(cat $crash_PATH/pkg_epoch)"
            rlAssertEquals "kernel package version" "$kernel_version" "$(cat $crash_PATH/pkg_version)"
            rlAssertEquals "kernel package release" "$kernel_release" "$(cat $crash_PATH/pkg_release)"
            rlAssertEquals "kernel package arch" "$kernel_arch" "$(cat $crash_PATH/pkg_arch)"
            rlAssertEquals "kernel package" "$( rpm -qf --qf %{name}-%{version}-%{release} /boot/vmlinuz-$rpm_version )" "$(cat $crash_PATH/package)"
            rlRun "cat $crash_PATH/pkg_fingerprint"

            rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        done
    rlPhaseEnd

    rlPhaseStartTest "Drop unreliable OOPS"
        prepare
        check_prior_crashes

        rlRun "ABRT_DROP_OOPS_VAL=\"$(augtool get /files/etc/abrt/plugins/oops.conf/DropNotReportableOopses | cut -d' ' -f3)\"" 0
        rlRun "augtool set /files/etc/abrt/plugins/oops.conf/DropNotReportableOopses yes" 0

        installed_kernel="$( rpm -q kernel | tail -n1 )"
        oops=oops_not_reportable_no_reliable_frame.test
        sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" $oops
        rlRun "abrt-dump-oops $oops -xD 2>&1 | grep 'abrt-dump-oops: Found oopses: [1-9]'" 0 "[$oops] Found OOPS"

        # abrtd does not notify that a problem has been detected and deleted.
        sleep 3

        rlRun "ls $ABRT_CONF_DUMP_LOCATION/oops* 2>&1 | grep -q 'No such file or directory'"

        if [ -n "$ABRT_DROP_OOPS_VAL" ]; then
            rlRun "augtool set /files/etc/abrt/plugins/oops.conf/DropNotReportableOopses $ABRT_DROP_OOPS_VAL" 0
        else
            rlRun "augtool rm /files/etc/abrt/plugins/oops.conf/DropNotReportableOopses" 0
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(echo *_ls)
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
