#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of compat-cores
#   Description: Tests correctness of creating user cores
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
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

TEST="compat-cores"
PACKAGE="abrt"
SUIDEDEXE="suidedexecutable"
CFG_FILE="/etc/abrt/abrt-action-save-package-data.conf"

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        rlFileBackup $CFG_FILE

        old_ulimit=$(ulimit -c)
        rlRun "ulimit -c unlimited" 0

        TmpDir=$(mktemp -d)
        chmod a+rwx $TmpDir
        cp loop.c $TmpDir
        pushd $TmpDir
        rlRun "useradd abrt-suid-test -M" 0
        rlRun "echo \"kokotice\" | passwd abrt-suid-test --stdin"

        sed -i 's/\(ProcessUnpackaged\) = no/\1 = yes/g' $CFG_FILE

    rlPhaseEnd

    rlPhaseStartTest "not setuid dump"
        prepare

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Make sure we use default suid_dumplable"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l`"
        rlRun '[ "xabrt-suid-test" == "x$(ls -l | grep "core.$killpid" | cut -d" " -f3)" ]' 0 "Checking if core is owned by abrt-suid-test"

        rlRun "abrt-cli rm $crash_PATH"
        rlRun "rm core.$killpid"
    rlPhaseEnd

    rlPhaseStartTest "not setuid dump - permission denied for other user files"
        prepare

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Make sure we use default suid_dumplable"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"

        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Create an artificial core dump file."
        rlRun "touch core.$killpid"

        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l`"
        rlAssertEquals "Checking if core has 0 Bytes" "x$(ls -l core.$killpid | cut -f5 -d' ')" "x0"
        rlRun "journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since=\"$SINCE\" | grep \"Can't open.*core.$killpid.*at '.*': Permission denied\""

        rlRun "abrt-cli rm $crash_PATH"
        rlRun "rm core.$killpid"
    rlPhaseEnd

    rlPhaseStartTest "not setuid dump - don't override other user files"
        prepare

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Make sure we use default suid_dumplable"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        ./$SUIDEDEXE &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Create an artificial core dump file."
        rlRun "touch core.$killpid"
        rlRun "chown abrt-suid-test:abrt-suid-test core.$killpid"

        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l`"
        rlAssertEquals "Checking if core has 0 Bytes" "x$(ls -l core.$killpid | cut -f5 -d' ')" "x0"
        rlRun "journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since=\"$SINCE\" | grep \".*core.$killpid.* at '.*' is not a regular file with link count 1 owned by UID(0)\""

        rlRun "abrt-cli rm $crash_PATH"
        rlRun "rm core.$killpid"
    rlPhaseEnd

    rlPhaseStartTest "not setuid dump - don't override hard links"
        prepare

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Make sure we use default suid_dumplable"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        ./$SUIDEDEXE &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Create an artificial core dump file."
        rlRun "echo all > denied.conf"
        rlRun "ln denied.conf core.$killpid"

        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l`"
        rlAssertEquals "Checking if denied.conf is untouched" "x$(cat denied.conf)" "xall"
        rlRun "journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since=\"$SINCE\" | grep \".*core.$killpid.* at '.*' is not a regular file with link count 1 owned by UID(0)\""

        rlRun "abrt-cli rm $crash_PATH"
        rlRun "rm denied.conf core.$killpid"
    rlPhaseEnd

    rlPhaseStartTest "secure setuid dump - no dump due to relative path in core_pattern"
        prepare

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 2 > /proc/sys/fs/suid_dumpable" 0 "Set setuid secure dump"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        rlRun "chmod u+s $SUIDEDEXE"
        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        SINCE=$(date +"%Y-%m-%d %T")
        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l`"
        rlAssertNotExist 'core.$killpid' 0 "Checking if core does not exist"
        rlRun "journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since=\"$SINCE\" | grep \"Current suid_dumpable policy prevents from saving core dumps according to relative core_pattern\""

        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Set setuid no dump"
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "secure setuid dump - no override files"
        prepare

        rlLog "Use absolute path in core_pattern"
        rlRun "SAVED_CORE=\"$(cat /var/run/abrt/saved_core_pattern)\""
        rlRun "CURRENT_CORE=\"$(cat /proc/sys/kernel/core_pattern)\""
        rlRun "systemctl stop abrt-ccpp"
        rlRun "OLD_CORE=\"$(cat /proc/sys/kernel/core_pattern)\""
        if [ "_${SAVED_CORE}" == "_${CURRENT_CORE}" ]; then
            rlLog "Reset OLD_CORE to 'core'"
            OLD_CORE="core"
        fi
        rlRun "echo /var/tmp/core.%p > /proc/sys/kernel/core_pattern"
        rlRun "systemctl start abrt-ccpp"

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 2 > /proc/sys/fs/suid_dumpable" 0 "Set setuid secure dump"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        rlRun "chmod u+s $SUIDEDEXE"
        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        SINCE=$(date +"%Y-%m-%d %T")

        rlLog "Create an artificial core dump file."
        rlRun "touch /var/tmp/core.$killpid"

        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l /var/tmp/core.*`"
        rlAssertEquals "Checking if core has 0 Bytes" "x$(ls -l /var/tmp/core.$killpid | cut -f5 -d' ')" "x0"
        rlRun "journalctl SYSLOG_IDENTIFIER=abrt-hook-ccpp --since=\"$SINCE\" | grep \"Can't open.*/var/tmp/core.$killpid.*at '.*': File exists\""

        rlRun "rm /var/tmp/core.$killpid"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Set setuid no dump"
        rlRun "systemctl stop abrt-ccpp"
        rlRun "echo \"$OLD_CORE\" > /proc/sys/kernel/core_pattern"
        rlRun "systemctl start abrt-ccpp"
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartTest "secure setuid dump"
        prepare

        rlLog "Use absolute path in core_pattern"
        rlRun "SAVED_CORE=\"$(cat /var/run/abrt/saved_core_pattern)\""
        rlRun "CURRENT_CORE=\"$(cat /proc/sys/kernel/core_pattern)\""
        rlRun "systemctl stop abrt-ccpp"
        rlRun "OLD_CORE=\"$(cat /proc/sys/kernel/core_pattern)\""
        if [ "_${SAVED_CORE}" == "_${CURRENT_CORE}" ]; then
            rlLog "Reset OLD_CORE to 'core'"
            OLD_CORE="core"
        fi
        rlRun "echo /var/tmp/core.%p > /proc/sys/kernel/core_pattern"
        rlRun "systemctl start abrt-ccpp"

        rlLog "ulimit: `ulimit -c`"
        rlRun "echo 2 > /proc/sys/fs/suid_dumpable" 0 "Set setuid secure dump"
        rlLog "Generate crash"
        rlRun "gcc loop.c -o $SUIDEDEXE"
        rlRun "chmod a+s $SUIDEDEXE"

        su abrt-suid-test -c "./$SUIDEDEXE &" &
        killpid=`pidof $SUIDEDEXE`
        c=0;while [ ! -n "$killpid" ]; do sleep 0.1; killpid=`pidof $SUIDEDEXE`; let c=$c+1; if [ $c -gt 500 ]; then break; fi; done;
        rlRun "kill -SIGSEGV $killpid"

        wait_for_hooks
        get_crash_path

        rlLog "core: `ls -l /var/tmp/core.*`"
        rlRun '[ "xroot root" == "x$(ls -l /var/tmp/core.* | grep "/var/tmp/core.$killpid" | cut -d" " -f3,4)" ]' 0 "Checking if core is owned by root"

        rlRun "rm /var/tmp/core.$killpid"
        rlRun "echo 0 > /proc/sys/fs/suid_dumpable" 0 "Set setuid no dump"
        rlRun "systemctl stop abrt-ccpp"
        rlRun "echo \"$OLD_CORE\" > /proc/sys/kernel/core_pattern"
        rlRun "systemctl start abrt-ccpp"
        rlRun "abrt-cli rm $crash_PATH"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore # CFG_FILE
        rlRun "ulimit -c $old_ulimit" 0
        rlRun "userdel -r -f abrt-suid-test" 0

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
