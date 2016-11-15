#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-action-find-bodhi-update
#   Description: Verify abrt-action-find-bodhi-update sanity
#   Author: Matej Habrnal <mhabrnal@redhat.com
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

TEST="abrt-action-find-bodhi-update"
PACKAGE="abrt"

FILENAME_DUPHASH="duphash"
FILENAME_OSINFO="os_info"
OSINFO_BUGZILLA_PRODUCT="REDHAT_BUGZILLA_PRODUCT="
OSINFO_BUGZILLA_PRODUCT_VERSION="REDHAT_BUGZILLA_PRODUCT_VERSION="

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "abrt-action-find-bodhi-update --help"
        rlRun "abrt-action-find-bodhi-update --help 2>&1 | grep 'usage: abrt-action-find-bodhi-update'"
    rlPhaseEnd

    rlPhaseStartTest "Not problem directory"
        rlRun "abrt-action-find-bodhi-update &> not_problem_directory.log" 2 "Not problem directory"
        rlAssertGrep "Problem directory error: cannot open problem directory" not_problem_directory.log
        rlAssertGrep "is not a problem directory" not_problem_directory.log

        rlRun "abrt-action-find-bodhi-update -d . &> not_problem_directory_with_d.log" 2 "Not problem directory"
        rlAssertGrep "Problem directory error: cannot open problem directory" not_problem_directory_with_d.log
        rlAssertGrep "is not a problem directory" not_problem_directory_with_d.log
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - missing duphash file"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        rlRun "abrt-action-find-bodhi-update -d $crash_PATH &> missing_duphas.log" 2 "Missing duphash file"
        rlAssertGrep "Problem directory error: problem directory misses 'duphash'" missing_duphas.log
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - missing os_info file"
        # create duphash file in problem dir
        rlRun "abrt-action-generate-backtrace -d $crash_PATH" 0 "Generate backtrace in problem dir"
        rlRun "abrt-action-analyze-backtrace -d $crash_PATH" 0 "Analyze backtrace in problem dir"

        # teporarily erase os_info file
        rlRun "mv -f ${crash_PATH}/${FILENAME_OSINFO} ${crash_PATH}/${FILENAME_OSINFO}.backup" 0

        rlRun "abrt-action-find-bodhi-update -d $crash_PATH &> missing_os_info.log" 2 "Missing os_info file"
        rlAssertGrep "Problem directory error: problem directory misses 'os_info'" missing_os_info.log

        # restore os_info file
        rlRun "cp -f ${crash_PATH}/${FILENAME_OSINFO}.backup ${crash_PATH}/${FILENAME_OSINFO}" 0
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - testing_product in os_info"
        # set $OSINFO_BUGZILLA_PRODUCT to "testing_product" in os_info
        bugzilla_product=$(grep "$OSINFO_BUGZILLA_PRODUCT" "${crash_PATH}/${FILENAME_OSINFO}")
        sed -i "s/${bugzilla_product}/${OSINFO_BUGZILLA_PRODUCT}\"testing_product\"/g" "${crash_PATH}/${FILENAME_OSINFO}"
        rlAssertGrep "${OSINFO_BUGZILLA_PRODUCT}\"testing_product\"" "${crash_PATH}/${FILENAME_OSINFO}"

        rlRun "abrt-action-find-bodhi-update -vvv -d $crash_PATH &> testing_product.log" 0 "os_info product 'testing_product'"
        rlAssertGrep "Using product \"testing_product\"" testing_product.log
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - no product in os_info"
        # erase $OSINFO_BUGZILLA_PRODUCT from os_info, using product from /etc/os-release
        sed -i "s/${OSINFO_BUGZILLA_PRODUCT}\"testing_product\"//g" "${crash_PATH}/${FILENAME_OSINFO}"
        rlAssertNotGrep "${OSINFO_BUGZILLA_PRODUCT}" "${crash_PATH}/${FILENAME_OSINFO}"
        rlRun "abrt-action-find-bodhi-update -vvv -d $crash_PATH &> os_release_product.log" 0 "Product from /etc/os-release"
        rlAssertGrep "Using product '${OSINFO_BUGZILLA_PRODUCT}' from /etc/os-release." os_release_product.log
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - rawhide product version"
        # rawhide product version -> do not run abrt-bodhi
        # set $OSINFO_BUGZILLA_PRODUCT_VERSION to "rawhide" in os_info
        bugzilla_version=$(grep "$OSINFO_BUGZILLA_PRODUCT_VERSION" "${crash_PATH}/${FILENAME_OSINFO}")
        sed -i "s/${bugzilla_version}/${OSINFO_BUGZILLA_PRODUCT_VERSION}\"rawhide\"/g" "${crash_PATH}/${FILENAME_OSINFO}"
        rlAssertGrep "${OSINFO_BUGZILLA_PRODUCT_VERSION}\"rawhide\"" "${crash_PATH}/${FILENAME_OSINFO}"

        rlRun "abrt-action-find-bodhi-update -vvv -d $crash_PATH &> rawhide_version.log" 0 "Product from /etc/os-release"
        rlAssertGrep "Using product '${OSINFO_BUGZILLA_PRODUCT}' from /etc/os-release." rawhide_version.log
        rlAssertGrep "Warning: abrt-bodhi do not support Product version 'Rawhide'" rawhide_version.log
    rlPhaseEnd

    rlPhaseStartTest "Loading files from problem directory - product from environment variable"
        # set product via environment variable 'Bugzilla_Product'
        rlRun "export Bugzilla_Product=\"foo\""
        rlRun "abrt-action-find-bodhi-update -vvv -d $crash_PATH &> env_product.log" 0 "Product from env"
        rlAssertGrep "Using product foo" env_product.log
        rlRun "unset Bugzilla_Product"
    rlPhaseEnd

    rlPhaseStartCleanup
        rm -rf $crash_PATH
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
