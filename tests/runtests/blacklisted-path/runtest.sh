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
        rlRun "mkdir -p /var/blah"
        rlRun "semanage fcontext -a -t bin_t '/var/blah(/.*)?'"
        rlRun "restorecon -RvF /var/blah"
    rlPhaseEnd

    rlPhaseStartTest "BlackListedPaths in effect"
        for cfg_file in $(echo $LOCAL_CFGS); do
            mv -f $cfg_file "/etc/abrt/$CFG_FNAME"
            BLACKLISTEDPATHS="$(grep BlackListedPaths /etc/abrt/abrt-action-save-package-data.conf |  sed 's/BlackListedPaths\s*=\s*//' )"
            rlLogInfo "Blacklisted paths: $BLACKLISTEDPATHS"

            # the copied config files has wrong selinux context => fixing..
            rlRun "restorecon -R /etc/abrt"

            rlLog "Generate crash (not blacklisted path)"
            prepare

            cp $( which will_abort ) /var/blah
            /var/blah/will_abort

            wait_for_hooks
            get_crash_path

            if [ -n "$crash_PATH" ]; then
                rlRun "abrt info $crash_PATH"
                remove_problem_directory
            fi

            rlLog "Generate second crash (blacklisted path)"
            prepare
            generate_crash
            # can't use wait_for_hooks as the path is blacklisted
            # and processing is stopped in post-create hook
            sleep 0.5

            rlAssert0 "No crash recorded" $(abrt status --bare)
        done
    rlPhaseEnd

    rlPhaseStartTest "empty BlackListedPaths"
        # 'will-crash' pack isn't signed on RHEL
        echo "OpenGPGCheck = no" > "/etc/abrt/$CFG_FNAME"

        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        if [ -n "$crash_PATH" ]; then
            rlRun "abrt info $crash_PATH"
            remove_problem_directory
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rm -rf /var/blah"
        rlFileRestore
        popd #TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
