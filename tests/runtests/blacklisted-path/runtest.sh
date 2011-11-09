#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of blacklisted-path
#   Description: tests if executable in blacklisted path is not being caught by abrt
#   Author: Richard Marko <rmarko@redhat.com>
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

TEST="blacklisted-path"
PACKAGE="abrt"

LOCAL_CFGS="conf_*"
CFG_FNAME="abrt-action-save-package-data.conf"

rlJournalStart
    rlPhaseStartSetup
        rlAssert0 "No prior crashes recorded" $(abrt-cli list | wc -l)
        if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
            rlDie "Won't proceed"
        fi

        TmpDir=$(mktemp -d)
        for cfg_file in $(echo $LOCAL_CFGS); do
            cp -v $cfg_file $TmpDir
        done
        pushd $TmpDir
        rlFileBackup "/etc/abrt/$CFG_FNAME"
    rlPhaseEnd

    rlPhaseStartTest "BlackListedPaths in effect"
        for cfg_file in $(echo $LOCAL_CFGS); do
            mv -f $cfg_file "/etc/abrt/$CFG_FNAME"
            BLACKLISTEDPATHS="$(grep BlackListedPaths /etc/abrt/abrt-action-save-package-data.conf |  sed 's/BlackListedPaths\s*=\s*//' )"
            rlLogInfo "Blacklisted paths: $BLACKLISTEDPATHS"

            rlLog "Generate crash (not blacklisted path)"
            sleep 3m &
            sleep 2
            kill -SIGSEGV %1
            sleep 3

            crash_PATH=$(abrt-cli list -f | grep Directory | tail -n1 | awk '{ print $2 }')
            if [ ! -d "$crash_PATH" ]; then
                rlDie "No crash dir generated, this shouldn't happen"
            fi
            rlLog "PATH = $crash_PATH"
            rlRun "abrt-cli info $crash_PATH"
            rlRun "abrt-cli rm $crash_PATH"

            rlLog "Generate second crash (blacklisted path)"
            yes > /dev/null &
            sleep 2
            kill -SIGSEGV %1
            sleep 3
            rlAssert0 "No crash recorded" $(abrt-cli list | wc -l)

            rlLog "Sleeping for 30 seconds"
            sleep 30
        done
    rlPhaseEnd

    rlPhaseStartTest "empty BlackListedPaths"
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 3

        crash_PATH=$(abrt-cli list -f | grep Directory | tail -n1 | awk '{ print $2 }')
        if [ ! -d "$crash_PATH" ]; then
            rlDie "No crash dir generated, this shouldn't happen"
        fi
        rlLog "PATH = $crash_PATH"
        rlRun "abrt-cli info $crash_PATH"
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        popd #TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
