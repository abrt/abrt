#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

TEST="abrtd-inotify-flood"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartTest
        rlServiceStop abrtd
        rlRun "abrtd -d >abrtd.LOG 2>&1 &" 0 "Start abrtd"
        sleep 1 # give it time to initialize
        # One second of intense inotify notifications:
        rlRun "./inotify_flooder.sh" 0 "Create/delete many files in /var/spool/abrtd"
        # The bug was confusing abrtd with 0-byte inotify read.
        # It was not able to react to other events (such as SIGTERM).
        # Check this condition:
        rlRun "killall abrtd" 0 "Send TERM to abrtd"
        sleep 1
        rlRun "! pidof abrtd" 0 "abrtd doesn't run"
        # Read buffering of inotify fd was making us misinterpret
        # inotify data - wrongly seeing it as indication directory creations.
        # Then abrtd was failing to find them, and declaring them 'corrupt':
        rlAssertNotGrep "Corrupted or bad directory" abrtd.LOG
        rm abrtd.LOG
    rlPhaseEnd

    rlPhaseStartCleanup
    rlServiceRestore abrtd
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
