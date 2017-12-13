#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bugzilla-dupe-search
#   Description: Tests reporter-bugzilla duplicate search
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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
. ../aux/lib.sh

TEST="bugzilla-default-url"
PACKAGE="abrt"
RPMNAME="libreport-plugin-bugzilla"
FILENAME_OSRELEASE='os_info'
BZ_FAKE_INSTANCE='http://127.0.0.1:12345'

rlJournalStart
    rlPhaseStartSetup
        rpm -q ${RPMNAME} || rlDie "Package '${RPMNAME}' is not installed."

        # Run the test in a temporary directory.
        TmpDir=$(mktemp -d)
        cp bugzilla.conf $TmpDir
        cp -r problem_dir $TmpDir
        pushd $TmpDir
        crash_PATH=problem_dir
    rlPhaseEnd

    rlPhaseStartTest "Check /etc/os-release for default url"
        rm -rf ${crash_PATH}/reported_to

        sed -i "/^BUG_REPORT_URL=/d" "${crash_PATH}/${FILENAME_OSRELEASE}"
        echo "BUG_REPORT_URL=${BZ_FAKE_INSTANCE}" >> "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla -v -c bugzilla.conf -d ${crash_PATH} 2>&1 | tee  output-local-bugzilla.log"

        rlAssertGrep "Failed to connect to 127.0.0.1 port 12345" output-local-bugzilla.log
        rlAssertNotExists ${crash_PATH}/reported_to

    rlPhaseEnd

    rlPhaseStartTest "os-release has undef default bug url"
        rm -rf ${crash_PATH}/reported_to

        sed -i "s/^\(BUG_REPORT_URL\)=.*/\1 = /" "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla -v -c bugzilla.conf -d ${crash_PATH} 2>&1 | tee  output-not-set.log" 0 "No bugzilla url in os-release, defaults to bugzilla.redhat.com"
        rlAssertGrep "Status: [A-Z _]* https://bugzilla.redhat.com/show_bug.cgi?id=1410984" output-not-set.log

    rlPhaseEnd

    rlPhaseStartTest "os-release has no default bug url"
        rm -rf ${crash_PATH}/reported_to

        sed -i "/^BUG_REPORT_URL=/d" "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir 2>&1 | tee output-no-url-option.log" 0 "No bugzilla url in os-release, defaults to bugzilla.redhat.com"
        rlAssertGrep "Status: [A-Z _]* https://bugzilla.redhat.com/show_bug.cgi?id=1410984" output-no-url-option.log

    rlPhaseEnd

    rlPhaseStartCleanup
        # Bundle logs. All files with the .log suffix will be bundled.
        rlBundleLogs abrt $(echo *.log)

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
