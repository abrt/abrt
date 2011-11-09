#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of systemd-init
#   Description: test startup via systemd
#   Author: Michal Nowak <mnowak@redhat.com>, Richard Marko <rmarko@redhat.com>
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

TEST="systemd-init"
PACKAGE="abrt"
SERVICE="${PACKAGE}d.service"

SCTL="/bin/systemctl"

rlJournalStart
    rlPhaseStartSetup "Prepare"
        if [ -x $STCL ]; then
            rlLog "systemd found"
        else
            rlDie "systemd not present on this system"
        fi
        killall abrtd
        rm -rf /var/run/abrt
    rlPhaseEnd

    rlPhaseStartTest "Start"
        rlRun "$SCTL start $SERVICE" 0
        rlRun "$SCTL status $SERVICE" 0
        rlRun "$SCTL start $SERVICE" 0
        rlRun "$SCTL status $SERVICE" 0
        rlRun "$SCTL restart $SERVICE" 0
        rlRun "$SCTL status $SERVICE" 0
    rlPhaseEnd

    rlPhaseStartTest "Stop"
        rlRun "$SCTL stop $SERVICE" 0
        sleep 3
        rlRun "$SCTL status $SERVICE" 3
        rlRun "$SCTL stop $SERVICE" 0
        rlRun "$SCTL status $SERVICE" 3
    rlPhaseEnd

    rlPhaseStartTest "PID File"
        rlRun "$SCTL start $SERVICE"
        sleep 3
        rlAssertExists "/var/run/abrtd.pid"
        rlRun "kill -n 9 $(cat /var/run/abrtd.pid)" 0
        sleep 3
        rlRun "$SCTL status $SERVICE" 3
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
