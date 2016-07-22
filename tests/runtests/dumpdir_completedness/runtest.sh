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
DDFILES="abrt_version analyzer architecture cmdline component count event_log executable hostname kernel last_occurrence machineid os_release package pkg_arch pkg_epoch pkg_name pkg_release pkg_version pkg_vendor pkg_fingerprint reason sosreport.tar.xz time type uid username uuid"

CCPP_FILES="core_backtrace coredump dso_list environ limits maps open_fds var_log_messages pid pwd cgroup"
PYTHON_FILES="backtrace"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes
        # install signed rpm because if the rpm is unsigned
        # pkg_fingerprint is not created
        rlRun "rpm -Uvh --force my_crash-0.0-1.el7.x86_64.rpm"

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin"
        prepare

        # crashing bin from my_crash package
        ccpp_crash

        wait_for_hooks
        get_crash_path

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        for FILE in $DDFILES $CCPP_FILES; do
            rlAssertExists "$crash_PATH/$FILE"
        done

        rlAssertGrep "/usr/sbin/ccpp_crash" "$crash_PATH/core_backtrace"

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Python plugin"
        prepare

        # crashing bin from my_crash package
        python_crash

        wait_for_hooks
        get_crash_path

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        for FILE in $DDFILES $PYTHON_FILES; do
            rlAssertExists "$crash_PATH/$FILE"
        done

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "rpm -e my_crash"
        rlBundleLogs abrt $(echo *_ls)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
