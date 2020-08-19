#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of blacklisted-package
#   Description: tests if blacklisted package is not being caught by abrt
#   Author: Michal Nowak <mnowak@redhat.com>
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

TEST="blacklisted-package"
PACKAGE="abrt"

CFG_FNAME="abrt-action-save-package-data.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp $CFG_FNAME $TmpDir
        pushd $TmpDir
        rlFileBackup "/etc/abrt/$CFG_FNAME"
        mv -f $CFG_FNAME "/etc/abrt/"

        BLACKLISTEDPKGS="$(grep -w BlackList /etc/abrt/abrt-action-save-package-data.conf | tee $TmpDir/blacklisted.abrt)"
        if [ ! -z "$BLACKLISTEDPKGS" ]; then
            rlLog "$(echo Blacklisted packages: $BLACKLISTEDPKGS)"
        else
            rlDie "No blacklisted packages; won't proceed."
        fi
        rlAssertGrep "strace" $TmpDir/blacklisted.abrt

        # the copied config files has wrong selinux context => fixing..
        rlRun "restorecon -R /etc/abrt"
    rlPhaseEnd

    rlPhaseStartTest
        prepare

        strace sleep 3m 2>&1 > /dev/null &
        sleep 1
        straced_sleep_pid=$( pstree -p -c -l -A | grep strace | sed 's#.*sleep(\(.*\))#\1#' )
        kill -11 $straced_sleep_pid

        sleep 2

        rlRun "abrt list --format={executable} | grep strace" 1 "No strace in abrt output"
        rlRun "abrt list --format={executable} | grep sleep" 1 "No sleep in abrt output"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        popd #TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
