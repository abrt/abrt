#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of duptest-core_backtrace
#   Description: Tests duplicate recognition based on core_backtrace similarity
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2014 Red Hat, Inc. All rights reserved.
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

TEST="duptest-core_backtrace"
PACKAGE="abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel
        load_abrt_conf
        rlRun "mv /etc/libreport/events.d/ccpp_event.conf /var/tmp/"
    rlPhaseEnd

    rlPhaseStartTest "Same CORE_BACKTRACEs & different UUIDs"
        prepare

        rlLog "Creating problem data."
        python <<EOF
import dbus
bus = dbus.SystemBus()

proxy = bus.get_object("org.freedesktop.problems", '/org/freedesktop/problems')

problems = dbus.Interface(proxy, dbus_interface='org.freedesktop.problems')

description = {"analyzer"    : "CCpp",
               "reason"      : "Application has been killed",
               "backtrace"   : "die()",
               "executable"  : "/usr/bin/true",
               "core_backtrace" :
"{\"signal\":11,\"stacktrace\":[{\"crash_thread\":true,\"frames\":[{\"address\":270434862256,\"build_id\":\"94dc0d88101e6afa78c2d7f799bce5dcdf74446f\",\"build_id_offset\":820400,\"function_name\":\"__nanosleep\",\"file_name\":\"/lib64/libc.so.6\",\"fingerprint\":\"6c1eb9626919a2a5f6a4fc4c2edc9b21b33b7354\"},{\"address\":4210271,\"build_id\":\"f84fbe616129d71ffe0ca3c05283a1928f0fdf67\",\"build_id_offset\":15967},{\"address\":1000,\"build_id_offset\":1000},{\"address\":0,\"build_id_offset\":0}]}]}" }

problems.NewProblem(description)
EOF
        wait_for_hooks

        rlRun "cd /var/tmp/abrt/CCpp*"

        first_occurrence=`cat last_occurrence`

        # Because of occurrence
        sleep 2

        prepare

        rlLog "Creating different problem data with same core_backtrace."
        python <<EOF
import dbus
bus = dbus.SystemBus()

proxy = bus.get_object("org.freedesktop.problems", '/org/freedesktop/problems')

problems = dbus.Interface(proxy, dbus_interface='org.freedesktop.problems')

description = {"analyzer"    : "CCpp",
               "reason"      : "Application has been killed again",
               "backtrace"   : "die_hard()",
               "executable"  : "/usr/bin/true",
               "core_backtrace" :
"{\"signal\":11,\"stacktrace\":[{\"crash_thread\":true,\"frames\":[{\"address\":270434862256,\"build_id\":\"94dc0d88101e6afa78c2d7f799bce5dcdf74446f\",\"build_id_offset\":820400,\"function_name\":\"__nanosleep\",\"file_name\":\"/lib64/libc.so.6\",\"fingerprint\":\"6c1eb9626919a2a5f6a4fc4c2edc9b21b33b7354\"},{\"address\":4210271,\"build_id\":\"f84fbe616129d71ffe0ca3c05283a1928f0fdf67\",\"build_id_offset\":15967},{\"address\":1000,\"build_id_offset\":1000},{\"address\":0,\"build_id_offset\":0}]}]}" }

problems.NewProblem(description)
EOF
        wait_for_hooks

        rlAssertNotEquals "Checking if last_occurrence has been updated" $first_occurrence `cat last_occurrence`
        rlAssertEquals "Checking if abrt counted only a single crash" `cat count` 2
    rlPhaseEnd

    rlPhaseStartCleanup
        # Not now, the conf file will be restored after a next case
        #rlRun "mv /var/tmp/ccpp_event.conf /etc/libreport/events.d/"
        rlRun "rm -rf /var/tmp/abrt/CCpp*" 0 "Removing problem dirs"
    rlPhaseEnd

    rlPhaseStartTest "Same CORE_BACKTRACEs & different EXECUTABLEs"
        prepare

        rlLog "Creating problem data."
        python <<EOF
import dbus
bus = dbus.SystemBus()

proxy = bus.get_object("org.freedesktop.problems", '/org/freedesktop/problems')

problems = dbus.Interface(proxy, dbus_interface='org.freedesktop.problems')

description = {"analyzer"    : "CCpp",
               "reason"      : "Application has been killed",
               "backtrace"   : "die()",
               "executable"  : "/usr/bin/sleep",
               "core_backtrace" :
"{\"signal\":11,\"stacktrace\":[{\"crash_thread\":true,\"frames\":[{\"address\":270434862256,\"build_id\":\"94dc0d88101e6afa78c2d7f799bce5dcdf74446f\",\"build_id_offset\":820400,\"function_name\":\"__nanosleep\",\"file_name\":\"/lib64/libc.so.6\",\"fingerprint\":\"6c1eb9626919a2a5f6a4fc4c2edc9b21b33b7354\"},{\"address\":4210271,\"build_id\":\"f84fbe616129d71ffe0ca3c05283a1928f0fdf67\",\"build_id_offset\":15967},{\"address\":1000,\"build_id_offset\":1000},{\"address\":0,\"build_id_offset\":0}]}]}" }

problems.NewProblem(description)
EOF
        wait_for_hooks

        rlRun "cd /var/tmp/abrt/CCpp*"

        first_occurrence=`cat last_occurrence`

        # Because of occurrence
        sleep 2

        prepare

        rlLog "Creating different problem data with same core_backtrace."
        python <<EOF
import dbus
bus = dbus.SystemBus()

proxy = bus.get_object("org.freedesktop.problems", '/org/freedesktop/problems')

problems = dbus.Interface(proxy, dbus_interface='org.freedesktop.problems')

description = {"analyzer"    : "CCpp",
               "reason"      : "Application has been killed",
               "backtrace"   : "die()",
               "executable"  : "/usr/bin/true",
               "core_backtrace" :
"{\"signal\":11,\"stacktrace\":[{\"crash_thread\":true,\"frames\":[{\"address\":270434862256,\"build_id\":\"94dc0d88101e6afa78c2d7f799bce5dcdf74446f\",\"build_id_offset\":820400,\"function_name\":\"__nanosleep\",\"file_name\":\"/lib64/libc.so.6\",\"fingerprint\":\"6c1eb9626919a2a5f6a4fc4c2edc9b21b33b7354\"},{\"address\":4210271,\"build_id\":\"f84fbe616129d71ffe0ca3c05283a1928f0fdf67\",\"build_id_offset\":15967},{\"address\":1000,\"build_id_offset\":1000},{\"address\":0,\"build_id_offset\":0}]}]}" }

problems.NewProblem(description)
EOF
        wait_for_hooks

        rlAssertEquals "Checking if last_occurrence has been updated" $first_occurrence `cat last_occurrence`
        rlAssertEquals "Checking if abrt counted only a single crash" `cat count` 1
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "mv /var/tmp/ccpp_event.conf /etc/libreport/events.d/"
        rlRun "rm -rf /var/tmp/abrt/CCpp*" 0 "Removing problem dirs"
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
