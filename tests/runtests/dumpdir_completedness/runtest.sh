#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dumpdir_completedness
#   Description: Tests basic functionality of dumpdir_completedness
#   Author: Martin Kyral <mkyral@redhat.com>
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

TEST="dumpdir_completedness"
PACKAGE="abrt"
DDFILES="abrt_version analyzer architecture cgroup cmdline component core_backtrace coredump count dso_list environ event_log executable global_pid hostname kernel last_occurrence limits machineid maps open_fds os_info os_release package pid pkg_arch pkg_epoch pkg_name pkg_release pkg_version proc_pid_status pwd reason runlevel sosreport.tar.xz time type uid username uuid var_log_messages"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin"
        generate_crash testuser
        wait_for_hooks
        get_crash_path

        ARCHITECTURE=$(rlGetPrimaryArch)

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        for FILE in $DDFILES ; do
            rlAssertExists "$crash_PATH/$FILE"
        done

        rlAssertGrep "/bin/will_segfault" "$crash_PATH/core_backtrace"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "ulimit -c $old_ulimit" 0
        rlBundleLogs abrt $(echo *_ls) $(echo verify_result*)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
