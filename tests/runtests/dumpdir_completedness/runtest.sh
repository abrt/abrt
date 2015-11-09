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
CCPPFILES="abrt_version analyzer architecture cgroup cmdline component core_backtrace coredump count dso_list environ event_log executable global_pid hostname kernel last_occurrence limits machineid maps open_fds os_info os_release package pid pkg_arch pkg_epoch pkg_name pkg_release pkg_version proc_pid_status pwd reason runlevel sosreport.tar.xz time type uid username uuid var_log_messages"
CCPPEXCL="sosreport.log"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        TmpDir=$(mktemp -d)
        cp ./sosreport.expected $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        # check the expected files
        for FILE in $CCPPFILES ; do
            rlAssertExists "$crash_PATH/$FILE"
        done
        # check the files expected to be gone
        for FILE in $CCPPEXCL ; do
            rlAssertiNotExists "$crash_PATH/$FILE"
        done
        # verify the contents
        rlAssertGrep "/bin/will_segfault" "$crash_PATH/core_backtrace"
        # verify the sosreport contents
        tar tvf $crash_PATH/sosreport.tar.xz | awk '{ print $6}' | sed 's/[^\/]*\//\//' | sed 's/\(rhsm-debug_system\).*/\1/' | sed 's/\(ld.so.conf.d\/\).*/\1*/' | sed 's/\(find_.lib.modules\).*/\1/' | sed 's/\(network-scripts\/\).*/\1*/' | sed 's/\(modules\/\).*/\1*/' | sed 's/\(\/sys\/devices\/system\/cpu\/\).*/\1*/' | sed 's/\(\/sys\/module\/\).*/\1*/' | sed 's/\(\/sys\/firmware\/\).*/\1*/' | sed 's/\(\/sys\/class\/\).*/\1*/' | sed 's/\(\/sys\/kernel\/\).*/\1*/' | sort | uniq > sosreport
        rlAssertNotDiffer sosreport $TmpDir/sosreport.expected


        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt $(echo *_ls)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
