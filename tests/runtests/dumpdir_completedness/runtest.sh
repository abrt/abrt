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

        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "CCpp plugin"
        prepare

        # jfilak: will-crash isn't always signed which is needed for pkg_fingerprint.
        #         If you want to use will-crash, do not forget to add EPEL key
        #         to /etc/abrt/gpg_keys.
        #generate_crash
        sleep 1000 &
        sleep 1
        kill -ABRT %1

        wait_for_hooks
        get_crash_path

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        for FILE in $DDFILES $CCPP_FILES; do
            rlAssertExists "$crash_PATH/$FILE"
        done

        rlAssertGrep "/bin/sleep" "$crash_PATH/core_backtrace"

        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Python plugin"
        prepare

        # jfilak: will-crash isn't always signed which is needed for pkg_fingerprint.
        #         If you want to use will-crash, do not forget to add EPEL key
        #         to /etc/abrt/gpg_keys.
        #will_python_raise
        #
        # Let's use a python file from yum which should be always installed
        #
        # $ find /usr/lib/python2.6/site-packages/yum -type f -name "*.py" -exec python '{}' \;
        # Jan 14 11:18:57 rhel6 abrt: detected unhandled Python exception in '/usr/lib/python2.6/site-packages/yum/mdparser.py'
        # Jan 14 11:18:57 rhel6 abrt: detected unhandled Python exception in '/usr/lib/python2.6/site-packages/yum/comps.py'
        # Jan 14 11:18:58 rhel6 abrt: detected unhandled Python exception in '/usr/lib/python2.6/site-packages/yum/pgpmsg.py'
        # Jan 14 11:18:58 rhel6 abrt: detected unhandled Python exception in '/usr/lib/python2.6/site-packages/yum/sqlitesack.py'
        # Jan 14 11:18:58 rhel6 abrt: detected unhandled Python exception in '/usr/lib/python2.6/site-packages/yum/repoMDObject.py'

        python /usr/lib/python2.6/site-packages/yum/mdparser.py
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
        rlBundleLogs abrt $(echo *_ls)
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
