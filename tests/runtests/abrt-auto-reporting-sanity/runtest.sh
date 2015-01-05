#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-auto-reporting-sanity
#   Description: does sanity on abrt-auto-reporting
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

TEST="abrt-auto-reporting-sanity"
PACKAGE="abrt"

function get_configured_value
{
    VALUE=`grep "^AutoreportingEnabled" /etc/abrt/abrt.conf | tr -d " " | cut -f2 -d "="`
    echo $VALUE
    case "$VALUE" in
        [yY][eE][sS]|"_")
            export CONF_VALUE="enabled"
            ;;
        [nN][oO])
            export CONF_VALUE="disabled"
            ;;
        *)
            echo "Unknown option value"
            export CONF_VALUE="disabled"
            ;;
    esac
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "--help"
        rlRun "abrt-auto-reporting --help" 0
        rlRun "abrt-auto-reporting --help 2>&1 | grep 'Usage: abrt-auto-reporting'"
    rlPhaseEnd

    rlPhaseStartTest "no args"
        rlRun "abrt-auto-reporting"

        get_configured_value
        rlAssertEquals "Reads the configuration" "_$(abrt-auto-reporting)" "_$CONF_VALUE"
    rlPhaseEnd

    rlPhaseStartTest "enabled"
        rlRun "abrt-auto-reporting enabled"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_enabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_enabled" "_$(abrt-auto-reporting)"
    rlPhaseEnd

    rlPhaseStartTest "disabled"
        rlRun "abrt-auto-reporting disabled"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_disabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_disabled" "_$(abrt-auto-reporting)"
    rlPhaseEnd

    rlPhaseStartTest "enabled (once more)"
        rlRun "abrt-auto-reporting enabled"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_enabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_enabled" "_$(abrt-auto-reporting)"
    rlPhaseEnd

    rlPhaseStartTest "various argument types"
        OLD="enabled"
        for arg in disabled EnAbLeD dIsAblEd enabled no Yes nO yes 0 1 off on
        do
            rlRun "abrt-auto-reporting $arg"

            get_configured_value
            rlAssertNotEquals "Changed the configuration" "_$OLD" "_$CONF_VALUE"

            if [ $CONF_VALUE != "enabled" ] && [ $CONF_VALUE != "disabled" ]; then
                rlFail "Mangles the configuration value"
            fi

            OLD=$CONF_VALUE
        done
    rlPhaseEnd

    rlPhaseStartTest "turn SSL Auth on"
        rlRun "abrt-auto-reporting --certificate rhsm"

        rlAssertGrep "^SSLClientAuth = rhsm$" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn HTTP Auth on"
        rlRun "abrt-auto-reporting --username rhn-username --password rhn-password"

        rlAssertGrep "^HTTPAuth = rhts-credentials$" /etc/libreport/plugins/ureport.conf
        rlAssertGrep "^Login = rhn-username$" /etc/libreport/plugins/rhtsupport.conf
        rlAssertGrep "^Password = rhn-password$" /etc/libreport/plugins/rhtsupport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn the Auth off"
        rlRun "abrt-auto-reporting --anonymous"

        rlAssertNotGrep "^SSLClientAuth" /etc/libreport/plugins/ureport.conf
        rlAssertNotGrep "^HTTPAuth" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText

rlJournalEnd

