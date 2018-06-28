#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-plugin-hook
#   Description: Testing functionality of abrt-hook-ccpp
#   Author: Matej Habrnal <mhabrnal@redhat.com>
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

TEST="ccpp-plugin-hook"
PACKAGE="abrt"

HOOK_PATH="/proc/sys/kernel/core_pattern"
TESTED_PATTERN="core.%p.%u.%g.%s.%t.%h.%e"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        rlRun "systemctl stop abrt-ccpp"

        rlLog "Backup core_pattern hook"
        HOOK_BCK=$( cat $HOOK_PATH )
        rlLog "Hook bck: '$HOOK_BCK'"
        rlLog "Tested hook: '$TESTED_PATTERN'"
        rlRun "sysctl kernel.core_pattern=${TESTED_PATTERN}"
        rlRun "systemctl start abrt-ccpp"

        CCORE_BCK=$(augtool get /files/etc/abrt/plugins/CCpp.conf/MakeCompatCore | cut -d'=' -f2 | tr -d ' ')
        rlLog "MakeCompatCore bck: '$HOOK_BCK'"
        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/MakeCompatCore yes"

        OGPGCH_BCK=$(augtool get /files/etc/abrt/abrt-action-save-package-data.conf/OpenGPGCheck | cut -d'=' -f2 | tr -d ' ')
        rlLog "MakeCompatCore bck: '$OGPGCH_BCK'"
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/OpenGPGCheck no"

        PUNPACKAGED_BCK=$(augtool get /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged | cut -d'=' -f2 | tr -d ' ')
        rlLog "MakeCompatCore bck: '$PUNPACKAGED_BCK'"
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged yes"

        ULIMIT_BCK=$(ulimit -c)
        rlLog "ulimit -c bck: '$ULIMIT_BCK'"
        rlRun "ulimit -c unlimited"

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        # create crashing binary which name contains space
        rlRun "cp /usr/bin/will_segfault crash\ binary"
    rlPhaseEnd

    rlPhaseStartTest
        prepare
        ./crash\ binary
        wait_for_hooks
        get_crash_path

        P_PID=$(cat $crash_PATH/pid | tr -d '\n') # core_pattern %p
        P_UID=$(cat $crash_PATH/uid | tr -d '\n') # core_pattern %u
        P_GID=$(id -g | tr -d '\n') # core_pattern %g
        P_SIG=11 # generate_crash calls will_segfault; core_pattern %s
        P_TIME=$(cat $crash_PATH/time | tr -d '\n') # core_pattern %t
        P_HOST=$(cat $crash_PATH/hostname | tr -d '\n') # core_pattern %h
        P_EXE=$(cat $crash_PATH/executable | tr -d '\n') # core_pattern %e

        ls -al
        # core file should exist
        rlAssertExists "core.${P_PID}.${P_UID}.${P_GID}.${P_SIG}.${P_TIME}.${P_HOST}.${P_EXE##*/}"

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
        rlRun "systemctl stop abrt-ccpp"
        rlRun "echo '$HOOK_BCK' > $HOOK_PATH" 0 "Restore old hook"
        rlRun "systemctl start abrt-ccpp"
        rlRun "augtool set /files/etc/abrt/plugins/CCpp.conf/MakeCompatCore $CCORE_BCK" 0 "Restore old MakeCompatCor"
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/OpenGPGCheck $OGPGCH_BCK"
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged $PUNPACKAGED_BCK"
        rlRun "ulimit -c $ULIMIT_BCK"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
