#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of oops_processing
#   Description: test for required files in dump directory of koops from journald
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2014 Red Hat, Inc. All rights reserved.
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

TEST="journal_oops_processing"
PACKAGE="abrt"
OOPS_REQUIRED_FILES="kernel uuid duphash
pkg_name pkg_arch pkg_epoch pkg_release pkg_version"
EXAMPLES_PATH="../../../examples"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        # the cut command removes syslog prefix
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops1.test \
            | cut -d" " -f6- > \
            $TmpDir/oops1.test

        # the cut command removes syslog prefix
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_no_reliable_frame.test \
            | cut -d" " -f6- > \
            $TmpDir/oops_not_reportable_no_reliable_frame.test

        sed "s/3.0.0-1.fc16.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops5.test > \
            TmpDir/oops5.test

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

        pushd $TmpDir

        rlRun "systemctl stop abrt-oops"

        # The stored cursor is not valid in testing configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dupm-journal-oops.state"
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            prepare

            installed_kernel="$( rpm -q kernel | tail -n1 )"
            kernel_version="$( rpm -q --qf "%{version}" $installed_kernel )"
            sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" $oops

            rlRun "ABRT_DUMP_JOURNAL_OOPS_DEBUG_FILTER=\"SYSLOG_IDENTIFIER=abrt_test\" abrt-dump-journal-oops -vvv -f -xD >$oops.log 2>&1 &"
            rlRun "ABRT_DUMPER_PID=$!"
            rlRun "logger -t abrt_test -f $oops"

            rlRun "sleep 1"

            rlAssertGrep "Found oopses: 1" $oops".log"

            wait_for_hooks
            get_crash_path

            ls $crash_PATH > crash_dir_ls

            for f in $OOPS_REQUIRED_FILES; do
                rlAssertExists "$crash_PATH/$f"
            done

            if [[ "$oops" == *not_reportable* ]]; then
                rlAssertExists "$crash_PATH/not-reportable"
            else
                rlAssertNotExists "$crash_PATH/not-reportable"
            fi

            rlAssertGrep "kernel" "$crash_PATH/pkg_name"
            rlAssertGrep "$kernel_version" "$crash_PATH/pkg_version"

            rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"

            # Kill the dumper with TERM to verify that it can store its state.
            # Next time, the dumper should start following the journald from
            # the last seen cursor.
            rlRun "kill -TERM $ABRT_DUMPER_PID"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        # Do not confuse the system dumper. The stored cursor is invalid in the default configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dupm-journal-oops.state"

        rlBundleLogs abrt $(echo *_ls)
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
