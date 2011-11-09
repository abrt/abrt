#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of check-hook-install
#   Description: Check whether ccpp-hook is installed and uninstalled properly
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

TEST="check-hook-install"
PACKAGE="abrt"

HOOK_PATH="/proc/sys/kernel/core_pattern"
NEW_HOOK="new_testing_hook"

rlJournalStart
    rlPhaseStartSetup
        rlLog "Backup core_pattern hook"
        HOOK_BCK=$( cat $HOOK_PATH )
        rlLog "Old hook: '$HOOK_BCK'"
        rlRun "echo '$NEW_HOOK' > $HOOK_PATH" 0 "Set new hook"
        rlLog "New hook: '$(cat $HOOK_PATH)'"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "abrt-install-ccpp-hook install" 0 "Install abrt hook"
        rlAssertNotGrep "$NEW_HOOK" "$HOOK_PATH"
        rlRun "abrt-install-ccpp-hook uninstall" 0 "Uninstall abrt hook"
        rlAssertGrep "$NEW_HOOK" "$HOOK_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "echo '$HOOK_BCK' > $HOOK_PATH" 0 "Restore old hook"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
