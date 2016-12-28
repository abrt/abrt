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
FILENAME_OSRELEASE='os_release'
BZ_FAKE_INSTANCE='http://127.0.0.1:12345'
BZ_INVALID_INSTANCE='http://a.555.444.333.222.111.000'

# use hash and conf from dup hash test.
#  undef BUG_REPORT_URL -> bugzilla.redhat.com
DUPHASH="5d25425e3589df3280ac8331554b2b1fa8384e49"

rlJournalStart
    rlPhaseStartSetup
        # Avoid interference with previous tests.
        check_prior_crashes

        rpm -q ${RPMNAME} || rlDie "Package '${RPMNAME}' is not installed."

        # Run the test in a temporary directory.
        TmpDir=$(mktemp -d)
        cp bugzilla.conf $TmpDir
        pushd $TmpDir
    rlPhaseEnd


    rlPhaseStartTest "Check /etc/os-release for default url"

        sed -i "s/BUG_REPORT_URL=.*//" "${crash_PATH}/${FILENAME_OSRELEASE}"
        echo "BUG_REPORT_URL=${BZ_FAKE_INSTANCE}" >> "${crash_PATH}/${FILENAME_OSRELEASE}"

        ./pyserve \
                version_response \
                0* \
                1no_duplicates_response \
                2bug_created_response \
                dummy \
                dummy \
                dummy \
                &> server_create &
        sleep 1

        rlRun "reporter-bugzilla -v -c bugzilla.conf -d problem_dir/ &> client_create"
        kill %1

        rlAssertGrep "http://localhost:12345/show_bug.cgi?id=1234567" client_create
        rm -f problem_dir/reported_to

    rlPhaseEnd

    rlPhaseStartTest "os-release has non-real default bug url"

        sed -i "s/BUG_REPORT_URL=.*//" "${crash_PATH}/${FILENAME_OSRELEASE}"
        echo "BUG_REPORT_URL=${BZ_FAKE_INSTANCE}" >> "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla --duphash=$DUPHASH -c bugzilla.conf > output" 1 "When os-release sets non-real id, reporter errors out"

    rlPhaseEnd

    rlPhaseStartTest "os-release has no default bug url"

        sed -i "s/BUG_REPORT_URL=.*//" "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla --duphash=$DUPHASH -c bugzilla.conf > output" 0 "No bugzilla url in os-release, defaults to bugzilla.redhat.com"
        rlAssertGrep "768647" output

    rlPhaseEnd

    rlPhaseStartTest "os-release has undef default bug url"

        sed -i "s/BUG_REPORT_URL=.*/BUG_REPORT_URL=''/" "${crash_PATH}/${FILENAME_OSRELEASE}"

        rlRun "reporter-bugzilla --duphash=$DUPHASH -c bugzilla.conf > output" 0 "No bugzilla url in os-release, defaults to bugzilla.redhat.com"
        rlAssertGrep "768647" output

    rlPhaseEnd


    rlPhaseStartCleanup
        # Bundle logs. All files with the .log suffix will be bundled.
        rlBundleLogs abrt $(echo *.log)

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
