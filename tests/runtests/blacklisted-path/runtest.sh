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
. ../aux/lib.sh

TEST="blacklisted-path"
PACKAGE="abrt"

LOCAL_CFGS="conf_*"
CFG_FNAME="abrt-action-save-package-data.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

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

            # the copied config files has wrong selinux context => fixing..
            rlRun "restorecon -R /etc/abrt"

            rlLog "Generate crash (not blacklisted path)"
            sleep 3m &
            sleep 2
            kill -SIGSEGV %1
            sleep 3

            crash_PATH=$(abrt-cli list -f | grep Directory | tail -n1 | awk '{ print $2 }')
            if [ ! -d "$crash_PATH" ]; then
                rlLog "No crash dir generated, this shouldn't happen"
            fi

            if [ -n "$crash_PATH" ]; then
                rlRun "abrt-cli info $crash_PATH"
                rlRun "abrt-cli rm $crash_PATH"
            fi

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
            rlLog "No crash dir generated, this shouldn't happen"
        fi
        rlLog "PATH = $crash_PATH"

        if [ -n "$crash_PATH" ]; then
            rlRun "abrt-cli info $crash_PATH"
            rlRun "abrt-cli rm $crash_PATH"
        fi

    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        popd #TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
