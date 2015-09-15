#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-auto-reporting-sanity-authenticated
#   Description: does sanity on abrt-auto-reporting-authenticated
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

TEST="abrt-auto-reporting-sanity-authenticated"
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

function turn_autoreporting_off
{
    rlLog "Disabling autoreporting"
    rlRun "abrt-auto-reporting disabled"

    get_configured_value
    rlAssertEquals "Saves the configuration" "_disabled" "_$CONF_VALUE"
    rlAssertEquals "Reads the configuration" "_disabled" "_$(abrt-auto-reporting)"
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "turn SSL Auth on"
        get_configured_value
        conf_value_old=$CONF_VALUE
        value_old=`abrt-auto-reporting`

        rlRun "abrt-auto-reporting --certificate rhsm"

        get_configured_value
        rlAssertEquals "Autoreporting option from conf file is not changed" \
                       "_$conf_value_old" "_$CONF_VALUE"
        rlAssertEquals "Autoreporting option from a-a-reporting is not changed" \
                       "_$value_old" "_$(abrt-auto-reporting)"

        rlAssertGrep "^SSLClientAuth = rhsm$" /etc/libreport/plugins/ureport.conf
        rlAssertGrep "^AuthDataItems = hostname, machineid$" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn SSL Auth on and enabled autoreporting"
        turn_autoreporting_off

        rlRun "abrt-auto-reporting 1 --certificate rhsm"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_enabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_enabled" "_$(abrt-auto-reporting)"

        rlAssertGrep "^SSLClientAuth = rhsm$" /etc/libreport/plugins/ureport.conf
        rlAssertGrep "^AuthDataItems = hostname, machineid$" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn HTTP Auth on"
        get_configured_value
        conf_value_old=$CONF_VALUE
        value_old=`abrt-auto-reporting`

        rlRun "abrt-auto-reporting --username rhn-username --password rhn-password"

        get_configured_value
        rlAssertEquals "Autoreporting option from conf file is not changed" \
                       "_$conf_value_old" "_$CONF_VALUE"
        rlAssertEquals "Autoreporting option from a-a-reporting is not changed" \
                       "_$value_old" "_$(abrt-auto-reporting)"

        rlAssertGrep "^HTTPAuth = rhts-credentials$" /etc/libreport/plugins/ureport.conf
        rlAssertGrep "^Login = rhn-username$" /etc/libreport/plugins/rhtsupport.conf
        rlAssertGrep "^Password = rhn-password$" /etc/libreport/plugins/rhtsupport.conf
        rlAssertGrep "^AuthDataItems = hostname, machineid$" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn HTTP Auth on and enabled autoreporting"
        turn_autoreporting_off

        rlRun "abrt-auto-reporting 1 --username rhn-username --password rhn-password"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_enabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_enabled" "_$(abrt-auto-reporting)"

        rlAssertGrep "^HTTPAuth = rhts-credentials$" /etc/libreport/plugins/ureport.conf
        rlAssertGrep "^Login = rhn-username$" /etc/libreport/plugins/rhtsupport.conf
        rlAssertGrep "^Password = rhn-password$" /etc/libreport/plugins/rhtsupport.conf
        rlAssertGrep "^AuthDataItems = hostname, machineid$" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn the Auth off"
        get_configured_value
        conf_value_old=$CONF_VALUE
        value_old=`abrt-auto-reporting`

        rlRun "abrt-auto-reporting --anonymous"

        get_configured_value
        rlAssertEquals "Autoreporting option from conf file is not changed" \
                       "_$conf_value_old" "_$CONF_VALUE"
        rlAssertEquals "Autoreporting option from a-a-reporting is not changed" \
                       "_$value_old" "_$(abrt-auto-reporting)"

        rlAssertNotGrep "^SSLClientAuth" /etc/libreport/plugins/ureport.conf
        rlAssertNotGrep "^HTTPAuth" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartTest "turn the anonymous autoreporting on"
        turn_autoreporting_off
        rlRun "abrt-auto-reporting 1 --anonymous"

        get_configured_value
        rlAssertEquals "Saves the configuration" "_enabled" "_$CONF_VALUE"
        rlAssertEquals "Reads the configuration" "_enabled" "_$(abrt-auto-reporting)"

        rlAssertNotGrep "^SSLClientAuth" /etc/libreport/plugins/ureport.conf
        rlAssertNotGrep "^HTTPAuth" /etc/libreport/plugins/ureport.conf
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-auto-reporting disabled"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText

rlJournalEnd

