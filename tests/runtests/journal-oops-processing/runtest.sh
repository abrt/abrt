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
ooPS_REQUIRED_FILES="kernel uuid duphash
pkg_name pkg_arch pkg_epoch pkg_release pkg_version"
EXAMPLES_PATH="../../../examples"

function test_single_oops
{
    oops="$1"
    shift
    exe="$1"
    shift
    args=$@

    if [ -z "$oops" ]; then
        rlDie "Need an oops file as the first command line argument"
    fi

    rlLog "Kernel oops ${oops}"

    if [ -z "$exe" ]; then
        exe="abrt-dump-journal-oops"
    fi

    prepare

    installed_kernel="$( rpm -q kernel | tail -n1 )"
    kernel_version="$( rpm -q --qf "%{version}" $installed_kernel )"
    sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" $oops
    sed -i "s/<KERNEL_VERSION>/$installed_kernel/g" ${oops/.test/.right}

    rlRun "journalctl --flush"

    rlRun "ABRT_DUMP_JOURNAL_OOPS_DEBUG_FILTER=\"SYSLOG_IDENTIFIER=abrt_test\" setsid ${exe} ${args} -vvv -f -xD -o >$oops.log 2>&1 &"
    rlRun "ABRT_DUMPER_PID=$!"

    rlRun "sleep 2"
    rlRun "logger -t abrt_test -f $oops"
    rlRun "journalctl --flush"

    rlRun "sleep 2"

    rlAssertGrep "Found oopses: 1" $oops".log"

    wait_for_hooks
    get_crash_path

    ls $crash_PATH > crash_dir_ls

    check_dump_dir_attributes $crash_PATH

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
    rlRun "killall -TERM abrt-dump-journal-oops"
    sleep 2

    if [ -d /proc/$ABRT_DUMPER_PID ]; then
        rlLogError "Failed to kill the abrt journal oops dumper"
        rlRun "kill -TERM -$ABRT_DUMPER_PID"
    fi

    cat $oops".log" \
        | grep -v "Version:" \
        | grep -v "Found oopses:" \
        | grep -v "abrt-dump-journal-oops:" \
        > $oops".right.log"

    rlRun "diff -u ${oops/.test/.right} ${oops}.right.log" 0 "The dumper copied oops data without any differences"
}


rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        # the cut command removes syslog prefix
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops1.test \
            | cut -d" " -f6- > \
            $TmpDir/oops1.test

        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops1.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops1.right

        # the cut command removes syslog prefix
        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_no_reliable_frame.test \
            | cut -d" " -f6- > \
            $TmpDir/oops_not_reportable_no_reliable_frame.test

        sed "s/2.6.27.9-159.fc10.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_no_reliable_frame.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops_not_reportable_no_reliable_frame.right

        sed "s/3.0.0-1.fc16.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops5.test > \
            $TmpDir/oops5.test

        sed "s/3.0.0-1.fc16.i686/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops5.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops5.right

        sed "s/3.10.0-33.el7.ppc64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops8_ppc64.test > \
            $TmpDir/oops8_ppc64.test

        sed "s/3.10.0-33.el7.ppc64/<KERNEL_VERSION>/" \
        $EXAMPLES_PATH/oops8_ppc64.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops8_ppc64.right

        sed "s/3.69.69-69.0.fit.s390x/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops10_s390x.test > \
            $TmpDir/oops10_s390x.test

        sed "s/3.69.69-69.0.fit.s390x/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops10_s390x.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops10_s390x.right

        sed "s/3.10.0-41.el7.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_unsupported_hw.test > \
            $TmpDir/oops_not_reportable_unsupported_hw.test

        sed "s/3.10.0-41.el7.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_unsupported_hw.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops_not_reportable_unsupported_hw.right

        sed "s/2.6.35.6-45.fc14.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_broken_bios.test > \
            $TmpDir/oops_not_reportable_broken_bios.test

        sed "s/2.6.35.6-45.fc14.x86_64/<KERNEL_VERSION>/" \
            $EXAMPLES_PATH/oops_broken_bios.right \
            | grep -v "abrt-dump-oops:" \
            | grep -v "Version:" > \
            $TmpDir/oops_not_reportable_broken_bios.right

        rlRun "cp abrt_dump_oops_in_fake_root $TmpDir"
        pushd $TmpDir

        rlRun "systemctl stop abrt-oops"

        # The stored cursor is not valid in testing configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dupm-journal-oops.state"
    rlPhaseEnd

    rlPhaseStartTest OOPS
        for oops in oops*.test; do
            test_single_oops ${oops}
        done
    rlPhaseEnd

    rlPhaseStartTest "Journals from a non-system directory"
        for oops in oops*.test; do
            rlRun "FakeRootDir=$(mktemp -d)"
            test_single_oops ${oops} ./abrt_dump_oops_in_fake_root ${FakeRootDir} -J /var/myjournal
            rlRun "rmdir ${FakeRootDir}"
        done
    rlPhaseEnd

    rlPhaseStartCleanup
        # Do not confuse the system dumper. The stored cursor is invalid in the default configuration.
        rlRun "rm -rf /var/lib/abrt/abrt-dupm-journal-oops.state"

        rlBundleLogs abrt $(echo *_ls) $(echo *.log)
        rlRun "popd"
        rlLog "$TmpDir"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
