#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of bz618602-core_pattern-handler-truncates-parameters
#   Description: Test whether core_pattern parameters are preserved
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

TEST="bz618602-core_pattern-handler-truncates-parameters"
PACKAGE="abrt"

CORENAME_MAX_SIZE=127
CORE_PATTERN="/proc/sys/kernel/core_pattern"

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        rlRun "cc -o $TmpDir/cppt ./core_pattern_pipe_test.c" 0 "Compile core_pattern_pipe_test"
        new_pattern="|$TmpDir/cppt %p %s %c %u %g %t " # !IMPORTANT: keep %p first (due to core_pattern_pipe_test implementation)
        to_fill=$[$CORENAME_MAX_SIZE-(${#new_pattern}+4)]
        pushd $TmpDir
        rlLog "Fill new pattern to 127 chars"
        for (( c=0; c<$to_fill; c++ )); do
            new_pattern="${new_pattern}|"
        done
        new_pattern="${new_pattern} abc" # put 'abc' to the end of the pattern


        old_pattern="$(cat $CORE_PATTERN)"
        rlLog "Using core_pattern of length ${#new_pattern}"
        rlLog "Pattern value: $new_pattern"
        rlRun "echo '$new_pattern' > $CORE_PATTERN" 0 "Setting new core_pattern"

        rlRun "ulimit -u unlimited" 0 "Set ulimit unlimited"
    rlPhaseEnd

    rlPhaseStartTest
        rlLog "Generate crash"
        sleep 3m &
        sleep 2
        kill -SIGSEGV %1
        sleep 5
        rlAssertExists "core.info"
        rlAssertGrep "<abc>" "core.info" # check presence of 'abc'
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "echo '$old_pattern' > $CORE_PATTERN" 0 "Restore core_pattern"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
