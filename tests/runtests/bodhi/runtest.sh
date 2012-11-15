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
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 python > output"
        rlAssertGrep 'package=python' request
        rlAssertGrep 'Accept: application/json' request

        rlAssertGrep 'python-2000.7-8.fc14.1' output
        rlAssertGrep 'mdadm-3000.2.2-13.fc16' output

        # not pretty printed query
        fake_serve ugly_python_query
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 python > output"
        rlAssertGrep 'package=python' request
        rlAssertGrep 'Accept: application/json' request

        rlAssertGrep 'python-2000.7-8.fc14.1' output
        rlAssertGrep 'mdadm-3000.2.2-13.fc16' output
    rlPhaseEnd

    rlPhaseStartTest "empty query"
        fake_serve empty_query
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 python > output"
        # TODO: fix missing No update found message
    rlPhaseEnd

    rlPhaseStartTest "invalid json"
        fake_serve invalid_json_query
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 python" 1
    rlPhaseEnd

    rlPhaseStartTest "memtest query"
        fake_serve memtest_query
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 memtest86+" 0
    rlPhaseEnd

    rlPhaseStartTest "release specific"
        fake_serve glusterfs_query_el6only
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 -rel6 glusterfs > output" 0

        rlAssertGrep 'release=el6' request
        rlAssertGrep 'glusterfs-3.2.5-4.el6' output
    rlPhaseEnd

    rlPhaseStartTest "by bug"
        fake_serve memtest_query
        rlRun "echo 'y' | abrt-bodhi -u http://localhost:12345 -b 123456,729197 > output" 0

        rlAssertGrep 'bugs=123456,729197' request
        rlAssertGrep 'memtest86+-4.20.4.fc16' output
    rlPhaseEnd

    rlPhaseStartCleanup
        killall nc
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
