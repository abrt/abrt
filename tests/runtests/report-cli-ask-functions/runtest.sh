#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of report-cli-ask-functions
#   Description: Verify report-cli-ask-functions functionality
#   Author: Matej Habrnal <mhabrnal@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc. All rights reserved.
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

TEST="report-cli-ask-functions"
PACKAGE="abrt"

QUERIES_DIR="."

rlJournalStart
    rlPhaseStartSetup
        LANG=""
        export LANG

        TmpDir=$(mktemp -d)

        EVENT_CONF_FILE='test_ask_event.conf'
        cp -v $EVENT_CONF_FILE /etc/libreport/events.d/
        cp -v expect $TmpDir

        ROOT_CONF_DIR='/root/.config/abrt/settings/'
        CONF_FILE='report-cli.conf'

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "ask_yes_no_save_result"
        generate_crash
        wait_for_hooks
        get_crash_path

        CONF_FILE_KEY='test_yes_no_save_result'

        if [ -e "$ROOT_CONF_DIR$CONF_FILE" ]; then
            rlRun "sed -i \"/$CONF_FILE_KEY/d\" $ROOT_CONF_DIR$CONF_FILE" 0 "Clean conf file $ROOT_CONF_DIR$CONF_FILE"
        else
            rlLog "Conf file $ROOT_CONF_DIR$CONF_FILE doesn't exist"
            touch $ROOT_CONF_DIR$CONF_FILE
        fi

        # user answer 'y'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_save_result $crash_PATH y 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log
        rlAssertNotGrep "$CONF_FILE_KEY" $ROOT_CONF_DIR$CONF_FILE

        # user answer 'N'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_save_result $crash_PATH n 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
        rlAssertNotGrep "$CONF_FILE_KEY" $ROOT_CONF_DIR$CONF_FILE

        # user answer 'f' (forever)
        # ./expect event dir response
        rlRun "./expect ask_yes_no_save_result $crash_PATH f 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE

        # should return YES without asking
        rlRun "report-cli -e ask_yes_no_save_result $crash_PATH 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log

        rlRun "sed -i \"/$CONF_FILE_KEY/d\" $ROOT_CONF_DIR$CONF_FILE" 0 "Clean conf file $ROOT_CONF_DIR$CONF_FILE"

        # user answer 'e' (never)
        # ./expect event dir response
        rlRun "./expect ask_yes_no_save_result $crash_PATH e 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
        rlAssertGrep "$CONF_FILE_KEY = no" $ROOT_CONF_DIR$CONF_FILE

        # should return NO without asking
        rlRun "report-cli -e ask_yes_no_save_result $crash_PATH 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
    rlPhaseEnd

    rlPhaseStartTest "ask_yes_no_yesforever"
        CONF_FILE_KEY='test_yes_no_yesforever'
        rlRun "sed -i \"/$CONF_FILE_KEY/d\" $ROOT_CONF_DIR$CONF_FILE" 0 "Clean conf file $ROOT_CONF_DIR$CONF_FILE"

        # user answer 'y'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH y 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE

        rlRun "sed -i \"/$CONF_FILE_KEY/d\" $ROOT_CONF_DIR$CONF_FILE" 0 "Clean conf file $ROOT_CONF_DIR$CONF_FILE"

        # user answer 'N'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH n 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE

        # user answer 'f' (forever)
        # save 'no' - 'no' means 'Don't ask me again, I said yes forever'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH f 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log
        rlAssertGrep "$CONF_FILE_KEY = no" $ROOT_CONF_DIR$CONF_FILE

        rlRun "report-cli -e ask_yes_no_yesforever $crash_PATH 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log

        # set 'test_yes_no_yesforever' to 'yes' - means yes ask me
        rlRun "sed -i \"/$CONF_FILE_KEY/d\" $ROOT_CONF_DIR$CONF_FILE" 0 "Clean conf file $ROOT_CONF_DIR$CONF_FILE"
        echo "$CONF_FILE_KEY = yes" >> $ROOT_CONF_DIR$CONF_FILE
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE

        # user answer 'y'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH y 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE

        # user answer 'N'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH N 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
        rlAssertGrep "$CONF_FILE_KEY = yes" $ROOT_CONF_DIR$CONF_FILE
    rlPhaseEnd

    rlPhaseStartTest "ask_yes_no"

        # user answer 'y'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH y 2>&1 > response.log" 0
        rlAssertGrep "test response YES" response.log
        rlAssertNotGrep "test response NO" response.log

        # user answer 'N'
        # ./expect event dir response
        rlRun "./expect ask_yes_no_yesforever $crash_PATH n 2>&1 > response.log" 0
        rlAssertGrep "test response NO" response.log
        rlAssertNotGrep "test response YES" response.log
    rlPhaseEnd

    rlPhaseStartTest "ask"

        # ./expect event dir response
        rlRun "./expect ask $crash_PATH login 2>&1 > response.log" 0
        rlAssertGrep "login" response.log
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH" 0 "Remove crash directory"
        rm -rf /etc/libreport/events.d/$EVENT_CONF_FILE
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
