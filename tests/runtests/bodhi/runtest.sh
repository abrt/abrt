#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bodhi
#   Description: Verify abrt-bodhi funcionality
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

TEST="bodhi"
PACKAGE="abrt"

function fake_serve {
    f="$1"
    echo "Serving $f on port 12345"
    { echo -ne "HTTP/1.1 200 OK\r\nContent-Length: $(wc -c < $f)\r\nContent-Type: application/json\r\n\r\n";
    cat $f; } | nc -l 12345 > request &
    sleep 1
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -R queries/* $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "sanity"
        rlRun "abrt-bodhi --help"
        rlRun "abrt-bodhi --help 2>&1 | grep 'Usage: abrt-bodhi'"
    rlPhaseEnd

    rlPhaseStartTest "simple run"
        fake_serve python_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 python &> output"

        cp request request.simple-run-nice.log
        rlAssertGrep 'GET /?packages=python' request
        rlAssertGrep 'Accept: application/json' request

        cp output output.simple-run-nice.log
        rlAssertGrep 'An update exists.*python-2000.7.12-1.fc24' output
        rlAssertGrep 'An update exists.*python3-3000.4.3-6.fc23' output
        rlAssertGrep 'An update exists.*python-docs-2.7.10-1.fc22' output

        # not pretty printed query
        fake_serve ugly_python_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 python &> output"

        cp request request.simple-run-ugly.log
        rlAssertGrep 'GET /?packages=python' request
        rlAssertGrep 'Accept: application/json' request

        cp output output.simple-run-ugly.log
        rlAssertGrep 'An update exists.*python-2000.7.12-1.fc24' output
        rlAssertGrep 'An update exists.*python3-3000.4.3-6.fc23' output
        rlAssertGrep 'An update exists.*python-docs-2.7.10-1.fc22' output
    rlPhaseEnd

    rlPhaseStartTest "empty query"
        fake_serve empty_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 python &> output"
        cp output output.empty.log
        # TODO: fix missing No update found message
    rlPhaseEnd

    rlPhaseStartTest "invalid json"
        fake_serve invalid_json_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 python &> output" 1
        cp output output.invalid.log
    rlPhaseEnd

    rlPhaseStartTest "memtest query"
        fake_serve memtest_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 memtest86+ &> output" 0

        cp output output.memtest-url-encode.log
        cp request request.memtest-url-encode.log
        rlAssertGrep 'GET /?packages=memtest86%2B' request
    rlPhaseEnd

    rlPhaseStartTest "release specific"
        fake_serve glusterfs_query_el6only
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 -rel6 glusterfs &> output" 0

        cp request request.glusterfs-release.log
        rlAssertGrep 'GET /?releases=el6&packages=glusterfs' request

        cp output output.glusterfs-release.log
        rlAssertGrep 'An update exists.*glusterfs-3000.2.5-4.el6' output
    rlPhaseEnd

    rlPhaseStartTest "by bug"
        fake_serve memtest_query
        rlRun "echo 'y' | abrt-bodhi -vvv -u http://localhost:12345 -b 1239675,1303804 &> output" 0

        cp request request.memtest-by-bug.log
        rlAssertGrep 'bugs=1239675,1303804' request

        cp output output.memtest-by-bug.log
        rlAssertGrep 'An update exists.*memtest86+-5.01-14.fc23' output
    rlPhaseEnd

    rlPhaseStartCleanup
        killall nc
        rlBundleLogs abrt-bodhi $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
