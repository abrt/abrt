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

LIBREPORT_EVENTS_D="/etc/libreport/events.d"
CCPP_EVENT_CONF=$LIBREPORT_EVENTS_D/post_create_will_abort_event.conf
PYTHON_EVENT_CONF=$LIBREPORT_EVENTS_D/post_create_will_python_raise_event.conf

# $1 - command
# $2 - grep pattern
# $3 - dump directories
# $4 - label
function test_run
{
    prepare

    last_line=`tail -1 /var/log/messages`
    $1
    wait_for_hooks

    sleep 1s

    # get only the new entries
    sed "1,/$last_line/ d" /var/log/messages 2>&1 | tee ${1}_$4.log
    rlAssertGrep "$2" ${1}_$4.log

    ps aux | grep abrt > ${1}_ps_$4.log
    rlAssertNotGrep "abrt-server" ${1}_ps_$4.log
    rlAssertNotGrep "abrt-event-handler" ${1}_ps_$4.log

    rlRun "killall abrt-event-handler" 1
    rlRun "killall abrt-server" 1

    rlLog "`ls -al $ABRT_CONF_DUMP_LOCATION`"

    rlAssertEquals "Dump directories" _$3 _`find $ABRT_CONF_DUMP_LOCATION -mindepth 1 -maxdepth 1 -type d | wc -l`

    service abrtd stop
    rm -rf $ABRT_CONF_DUMP_LOCATION
    service abrtd start
    service abrt-ccpp start
}

# $1 command
function test_no_debug
{
    test_run $1 "Removing problem provoked by ABRT(pid:.*): '$ABRT_CONF_DUMP_LOCATION/.*'" 1 NO_debug

}

# $1 command
function test_debug
{

    dump_dirs_count=${2:-2}
    test_run $1 "ABRTD_PID=.*;DUMP_DIR='$ABRT_CONF_DUMP_LOCATION/.*';EVENT='post-create';REASON='.*';CMDLINE='.*will_.*'" $dump_dirs_count debug
}

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf

        service abrtd stop
        rm -rf $ABRT_CONF_DUMP_LOCATION
        rlRun "augtool set /files/etc/abrt/abrt.conf/DebugLevel 0"

        cat > $CCPP_EVENT_CONF <<EOF
EVENT=post-create type=CCpp
    sleep 30
    echo "Starting loop ..."
    will_abort --random
    exit 0
EOF

        cat > $PYTHON_EVENT_CONF <<EOF
EVENT=post-create type=Python
    sleep 30
    echo "Starting loop ..."
    will_python_raise
    exit 0
EOF

        service abrtd start
        service abrt-ccpp start

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "C/C++ hook - no debug"
        test_no_debug will_segfault
    rlPhaseEnd

    rlPhaseStartTest "Python hook - no debug"
        test_no_debug will_python_raise
    rlPhaseEnd

    rlPhaseStartSetup "Set debug"
        service abrtd stop
        rm -rf $ABRT_CONF_DUMP_LOCATION
        rlRun "augtool set /files/etc/abrt/abrt.conf/DebugLevel 1"
        service abrtd start
        service abrt-ccpp start
    rlPhaseEnd

    rlPhaseStartTest "C/C++ hook - debug"
        test_debug will_segfault
    rlPhaseEnd

    rlPhaseStartTest "Python hook - debug"
        # the second problem should removed as dumplicate of the first one
        # because it is used the same crashing binary 'will_python_raise'
        test_debug will_python_raise 1
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "augtool set /files/etc/abrt/abrt.conf/DebugLevel 0"
        rlBundleLogs abrt $(ls *.log)
        popd # TmpDir
        rm -rf $TmpDir
        rm -f $CCPP_EVENT_CONF $PYTHON_EVENT_CONF
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
