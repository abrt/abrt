#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrtd-infinite-event-loop
#   Description: Tests ability to break infinite event loop
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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

TEST="abrtd-infinite-event-loop"
PACKAGE="abrt"

ABRT_CONF=/etc/abrt/abrt.conf
LIBREPORT_EVENTS_D="/etc/libreport/events.d"
CCPP_EVENT_CONF=$LIBREPORT_EVENTS_D/post_create_will_abort_event.conf
PYTHON3_EVENT_CONF=$LIBREPORT_EVENTS_D/post_create_will_python3_raise_event.conf

# $1 - command
# $2 - grep pattern
# $3 - dump directories
# $4 - label
function test_run
{
    prepare

    SINCE=$(date +"%Y-%m-%d %T")
    $1
    wait_for_hooks

    sleep 1s

    journalctl --since="$SINCE" 2>&1 | tee ${1}_$4.log
    rlAssertGrep "$2" ${1}_$4.log

    ps aux | grep abrt > ${1}_ps_$4.log
    rlAssertNotGrep "abrt-server" ${1}_ps_$4.log
    rlAssertNotGrep "abrt-event-handler" ${1}_ps_$4.log

    rlRun "killall abrt-event-handler" 1
    rlRun "killall abrt-server" 1

    rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

    rlAssertEquals "Dump directories" _$3 _`find $ABRT_CONF_DUMP_LOCATION -mindepth 1 -maxdepth 1 -type d | wc -l`

    systemctl stop abrtd
    rm -rf $ABRT_CONF_DUMP_LOCATION
    systemctl start abrtd
    systemctl start abrt-ccpp
}

# $1 command
function test_no_debug
{
    test_run $1 "Removing problem provoked by ABRT(pid:.*): '$ABRT_CONF_DUMP_LOCATION/.*'" 1 NO_debug

}

# $1 command
function test_debug
{
    test_run $1 "ABRT_SERVER_PID=.*;DUMP_DIR='$ABRT_CONF_DUMP_LOCATION/.*';EVENT='post-create';REASON='.*';CMDLINE='.*'" 2 debug
}

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf

        systemctl stop abrtd
        rm -rf $ABRT_CONF_DUMP_LOCATION
        rlRun "augtool set /files${ABRT_CONF}/DebugLevel 0"

        cat > $CCPP_EVENT_CONF <<EOF
EVENT=post-create type=CCpp
    sleep 30
    echo "Starting loop ..."
    will_abort --random
    exit 0
EOF

        cat > $PYTHON3_EVENT_CONF <<EOF
EVENT=post-create type=Python3
    sleep 30
    echo "Starting loop ..."
    python3 -c 'import os; os.kill(os.getpid(), 11)'
    exit 0
EOF

        systemctl start abrtd
        systemctl start abrt-ccpp

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "C/C++ hook - no debug"
        test_no_debug will_segfault
    rlPhaseEnd

    rlPhaseStartTest "Python3 hook - no debug"
        test_no_debug will_python3_raise
    rlPhaseEnd

    rlPhaseStartSetup "Set debug"
        systemctl stop abrtd
        rm -rf $ABRT_CONF_DUMP_LOCATION
        rlRun "augtool set /files${ABRT_CONF}/DebugLevel 1"
        systemctl start abrtd
        systemctl start abrt-ccpp
    rlPhaseEnd

    rlPhaseStartTest "C/C++ hook - debug"
        test_debug will_segfault
    rlPhaseEnd

    rlPhaseStartTest "Python3 hook - debug"
        test_debug will_python3_raise
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "augtool set /files${ABRT_CONF}/DebugLevel 0"
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
        rm -f $CCPP_EVENT_CONF $PYTHON3_EVENT_CONF
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
