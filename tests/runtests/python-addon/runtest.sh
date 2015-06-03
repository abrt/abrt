#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of python-addon
#   Description: tests the functionality of the ABRT Python exception handler
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

TEST="python-addon"
PACKAGE="abrt"

TFILE="/usr/bin/pydoc"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        pushd $TmpDir
        rlAssertRpm "abrt-addon-python"
        rlFileBackup $TFILE
        rlRun "python $TFILE" 0 "Run unmodified $TFILE"
    rlPhaseEnd

    rlPhaseStartTest
        rlRun "sed -i '2i 0/0 # error line' $TFILE" 0 "Add error line to $TFILE"
        cat $TFILE
        rlRun "python $TFILE" 1 "Run python $TFILE"
        sleep 3

        get_crash_path

        rlRun "echo $crash_PATH_RIGHTS | grep drwxr-x---" 0 "Crash directory has proper rights"
        rlRun "[ 'x$crash_PATH_USER' == 'xroot' ]" 0 "Crash directory owned by root"
        rlRun "[ 'x$crash_PATH_GROUP' == 'xabrt' ]" 0 "Crash directory group is abrt"

        rlRun "abrt-cli info $crash_PATH | grep $TFILE" 0 "abrt-cli info should contain $TFILE"
        rlRun "abrt-cli info $crash_PATH | grep 'Python'" 0 "abrt-cli info should contain 'Python'"

    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        rlRun "abrt-cli rm $crash_PATH"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
