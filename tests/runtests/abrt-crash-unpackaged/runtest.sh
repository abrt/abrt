#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-crash-unpackaged
#   Description: Tests basic functionality of problem direrctory creation for
#   unpackaged executables crashes.
#   Author: Julius Milan <jmilan@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2017 Red Hat, Inc. All rights reserved.
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
. ../aux/lib.sh

TEST="abrt-crash-unpackaged"
PACKAGE="abrt"


rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged yes"
    rlPhaseEnd

    rlPhaseStartTest "unpackaged binary problem dir check"
        prepare
        generate_crash_unpack
        wait_for_hooks
        get_crash_path

        rlAssertNotExists $crash_PATH/component
        rlAssertNotExists $crash_PATH/package
        rlAssertNotExists $crash_PATH/pkg_arch
        rlAssertNotExists $crash_PATH/pkg_epoch
        rlAssertNotExists $crash_PATH/pkg_fingerprint
        rlAssertNotExists $crash_PATH/pkg_name
        rlAssertNotExists $crash_PATH/pkg_release
        rlAssertNotExists $crash_PATH/pkg_vendor
        rlAssertNotExists $crash_PATH/pkg_version

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "packaged binary problem dir check"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlAssertExists $crash_PATH/component
        rlAssertExists $crash_PATH/package
        rlAssertExists $crash_PATH/pkg_arch
        rlAssertExists $crash_PATH/pkg_epoch
        rlAssertExists $crash_PATH/pkg_fingerprint
        rlAssertExists $crash_PATH/pkg_name
        rlAssertExists $crash_PATH/pkg_release
        rlAssertExists $crash_PATH/pkg_vendor
        rlAssertExists $crash_PATH/pkg_version

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged no"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
