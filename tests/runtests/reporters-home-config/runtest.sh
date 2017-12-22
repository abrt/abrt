#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of reporter-bugzilla, reporter-rhtsupport, reporter-mantisbt
#   Description: Verify their ability to read configuration from current user's home
#   Author: Julius Milan <jmilan@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2017 Red Hat, Inc. All rights reserved.
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

TEST="reporters-home-config"
PACKAGE="abrt"

TEST_DIR="."

GLOBAL_BUGZILLA_CONF=/etc/libreport/plugins/bugzilla.conf
GLOBAL_RHTSUPPORT_CONF=/etc/libreport/plugins/rhtsupport.conf
GLOBAL_MANTISBT_CONF=/etc/libreport/plugins/mantisbt.conf
LOCAL_BUGZILLA_CONF=$HOME/.config/libreport/bugzilla.conf
LOCAL_RHTSUPPORT_CONF=$HOME/.config/libreport/rhtsupport.conf
LOCAL_MANTISBT_CONF=$HOME/.config/libreport/mantisbt.conf

rlJournalStart
    rlPhaseStartSetup
        check_prior_crashes

        # ensure local config exists
        mkdir -p  ~/.config/libreport/
        touch ~/.config/libreport/bugzilla.conf
        touch ~/.config/libreport/rhtsupport.conf
        touch ~/.config/libreport/mantisbt.conf

        rlFileBackup $GLOBAL_BUGZILLA_CONF $GLOBAL_RHTSUPPORT_CONF $GLOBAL_MANTISBT_CONF \
            $LOCAL_BUGZILLA_CONF $LOCAL_RHTSUPPORT_CONF $LOCAL_MANTISBT_CONF

        # unset global configuration
        augtool clear /files${GLOBAL_BUGZILLA_CONF}/Login
        augtool clear /files${GLOBAL_BUGZILLA_CONF}/Password
        augtool clear /files${GLOBAL_RHTSUPPORT_CONF}/Login
        augtool clear /files${GLOBAL_RHTSUPPORT_CONF}/Password
        augtool clear /files${GLOBAL_MANTISBT_CONF}/Login
        augtool clear /files${GLOBAL_MANTISBT_CONF}/Password

        TmpDir=$(mktemp -d)
        cp -v $TEST_DIR/expect $TmpDir/expect
        pushd "$TmpDir"
    rlPhaseEnd

    rlPhaseStartTest "Check reporter-bugzilla, without local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # delete local configuration
        rlRun "augtool rm /files$LOCAL_BUGZILLA_CONF/Login" 0 "Delete local Login"
        rlRun "augtool rm /files$LOCAL_BUGZILLA_CONF/Password" 0 "Delete local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_BUGZILLA_CONF/Login global_user" 0 "Set global Login"
        rlRun "augtool clear /files$GLOBAL_BUGZILLA_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-bugzilla -v -d $crash_PATH &> out_bz_1" 1 "Try to report by reporter-bugzilla"

        # when there is no local config, global config should be used
        rlAssertNotGrep "Login is not provided by configuration." out_bz_1
        rlAssertGrep "Password is not provided by configuration. Please enter the password for 'global_user':" out_bz_1

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check reporter-bugzilla, with local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_BUGZILLA_CONF/Login local_user" 0 "Set local Login"
        rlRun "augtool set /files$LOCAL_BUGZILLA_CONF/Password bbb" 0 "Set local Password"
        # unset global configuration
        rlRun "augtool clear /files$GLOBAL_BUGZILLA_CONF/Login" 0 "Clear global Login"
        rlRun "augtool clear /files$GLOBAL_BUGZILLA_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-bugzilla -v -d $crash_PATH &> out_bz_2" 1 "Try to report by reporter-bugzilla"

        # check that local config is not ignored
        rlAssertNotGrep "Login is not provided by configuration." out_bz_2
        rlAssertNotGrep "Password is not provided by configuration." out_bz_2

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check config priority of reporter-bugzilla, with both global and local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_BUGZILLA_CONF/Login local_user" 0 "Set local Login"
        rlRun "augtool clear /files$LOCAL_BUGZILLA_CONF/Password" 0 "Clear local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_BUGZILLA_CONF/Login global_user" 0 "Set global Login"
        rlRun "augtool clear /files$GLOBAL_BUGZILLA_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-bugzilla -v -d $crash_PATH &> out_bz_3" 1 "Try to report by reporter-bugzilla"

        # when both configs are set, local config should be used
        # this is determined according to user for which it asks for password
        rlAssertNotGrep "Login is not provided by configuration." out_bz_3
        rlAssertGrep "Password is not provided by configuration. Please enter the password for 'local_user':" out_bz_3

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check reporter-rhtsupport, without local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # delete local configuration
        rlRun "augtool rm /files$LOCAL_RHTSUPPORT_CONF/Login" 0 "Delete local Login"
        rlRun "augtool rm /files$LOCAL_RHTSUPPORT_CONF/Password" 0 "Delete local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_RHTSUPPORT_CONF/Login global_user" 0 "Set global Login"
        rlRun "augtool clear /files$GLOBAL_RHTSUPPORT_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-rhtsupport -v -d $crash_PATH &> out_rhts_1" 69 "Try to report by reporter-rhtsupport"

        # when there is no local config, global config should be used
        rlAssertNotGrep "Login is not provided by configuration." out_rhts_1
        rlAssertGrep "Password is not provided by configuration. Please enter the password for 'global_user'" out_rhts_1

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check reporter-rhtsupport, with local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_RHTSUPPORT_CONF/Login local_user" 0 "Set local Login"
        rlRun "augtool set /files$LOCAL_RHTSUPPORT_CONF/Password bbb" 0 "Set local Password"
        # unset global configuration
        rlRun "augtool clear /files$GLOBAL_RHTSUPPORT_CONF/Login" 0 "Clear global Login"
        rlRun "augtool clear /files$GLOBAL_RHTSUPPORT_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-rhtsupport -v -d $crash_PATH &> out_rhts_2" 69 "Try to report by reporter-rhtsupport"

        # check that local config is not ignored
        rlAssertNotGrep "Login is not provided by configuration." out_rhts_2
        rlAssertNotGrep "Password is not provided by configuration." out_rhts_2

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check config priority of reporter-rhtsupport, with both global and local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_RHTSUPPORT_CONF/Login local_user" 0 "Set local Login"
        rlRun "augtool clear /files$LOCAL_RHTSUPPORT_CONF/Password" 0 "Clear local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_RHTSUPPORT_CONF/Login global_user" 0 "Set global Login"
        rlRun "augtool clear /files$GLOBAL_RHTSUPPORT_CONF/Password" 0 "Clear global Password"

        rlRun "yes no | reporter-rhtsupport -v -d $crash_PATH &> out_rhts_3" 69 "Try to report by reporter-rhtsupport"

        # when both configs are set, local config should be used
        # this is determined according to user for which it asks for password
        rlAssertNotGrep "Login is not provided by configuration." out_rhts_3
        rlAssertGrep "Password is not provided by configuration. Please enter the password for 'local_user':" out_rhts_3

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    # since reporter-mantisbt behavior is "all or nothing" in case of credentials, i.e.
    # doesn't accept single login or password from config, but requires both,
    # test logic is also a bit different

    rlPhaseStartTest "Check reporter-mantisbt, without local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # delete local configuration
        rlRun "augtool rm /files$LOCAL_MANTISBT_CONF/Login" 0 "Delete local Login"
        rlRun "augtool rm /files$LOCAL_MANTISBT_CONF/Password" 0 "Delete local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_MANTISBT_CONF/Login aaa" 0 "Set global Login"
        rlRun "augtool set /files$GLOBAL_MANTISBT_CONF/Password bbb" 0 "Set global Password"

        rlRun "./expect $crash_PATH &> out_mtbt_1" 0 "run reporter-mantisbt -v -d CRASH_DIR"

        # credentials are provided by global configuration
        rlAssertNotGrep "Credentials are not provided by configuration." out_mtbt_1

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check reporter-mantisbt, with local config"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_MANTISBT_CONF/Login local_user" 0 "Set local Login"
        rlRun "augtool set /files$LOCAL_MANTISBT_CONF/Password bbb" 0 "Set local Password"

        rlRun "./expect $crash_PATH &> out_mtbt_2" 0 "run reporter-mantisbt -v -d CRASH_DIR"

        # credentials are provided by local configuration
        rlAssertNotGrep "Credentials are not provided by configuration." out_mtbt_2

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check config priority of reporter-mantisbt, with both global and local config 1"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # unset local configuration
        rlRun "augtool clear /files$LOCAL_MANTISBT_CONF/Login" 0 "Clear local Login"
        rlRun "augtool clear /files$LOCAL_MANTISBT_CONF/Password" 0 "Clear local Password"
        # set global configuration
        rlRun "augtool set /files$GLOBAL_MANTISBT_CONF/Login aaa" 0 "Set global Login"
        rlRun "augtool set /files$GLOBAL_MANTISBT_CONF/Password bbb" 0 "Set global Password"

        rlRun "echo | reporter-mantisbt -v -d  $crash_PATH &> out_mtbt_3" 69 "run reporter-mantisbt -v -d CRASH_DIR"

        # global config is set but local config contains empty values, which
        # should override the global ones
        rlAssertGrep "Credentials are not provided by configuration." out_mtbt_3
        rlAssertNotGrep "Invalid password or login." out_mtbt_3

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartTest "Check config priority of reporter-mantisbt, with both global and local config 2"
        prepare
        generate_crash
        wait_for_hooks
        get_crash_path

        # set local configuration
        rlRun "augtool set /files$LOCAL_MANTISBT_CONF/Login aaa" 0 "Set local Login"
        rlRun "augtool set /files$LOCAL_MANTISBT_CONF/Password bbb" 0 "Set local Password"
        # unset global configuration
        rlRun "augtool clear /files$GLOBAL_MANTISBT_CONF/Login" 0 "Clear global Login"
        rlRun "augtool clear /files$GLOBAL_MANTISBT_CONF/Password" 0 "Clear global Password"

        rlRun "echo | reporter-mantisbt -v -d  $crash_PATH &> out_mtbt_4" 69 "run reporter-mantisbt -v -d CRASH_DIR"

        # local config is set, will provide some (invalid) credentials
        rlAssertNotGrep "Credentials are not provided by configuration." out_mtbt_4
        rlAssertGrep "Invalid password or login." out_mtbt_4

        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash dir"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlFileRestore
        rlBundleLogs abrt out_*
        popd # TmpDir
        rm -rf "$TmpDir"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
