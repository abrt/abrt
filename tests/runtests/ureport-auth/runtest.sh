#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of ureport-auth
#   Description: Test reporter-ureport with client authentication enabled
#   Author: Martin Milata <mmilata@redhat.com>
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

TEST="ureport-auth"
PACKAGE="abrt"

function run_reporter() {
    AUTH=$1
    ARGS=$2
    RET=$3

    ./pyserve $AUTH &> server_log &
    PYSERVE_PID=$!
    wait_for_server 12345

    rlRun "reporter-ureport -v --insecure --url https://localhost:12345/faf -d $crash_PATH $ARGS &> ccpp_reporter" $RET "auth $AUTH, reporter-ureport $ARGS"

    kill $PYSERVE_PID
}

rlJournalStart
    rlPhaseStartSetup
        TmpDir=$(mktemp -d)
        cp -r cert/ $TmpDir
        cp pyserve $TmpDir
        pushd $TmpDir

        check_prior_crashes
        generate_crash
        get_crash_path
    rlPhaseEnd

    rlPhaseStartTest "Server auth disabled"
        # server: auth disabled, client: auth disabled
        run_reporter none "" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth disabled, client: invalid cert path
        run_reporter none "-t /etc/doesnt.pem:/etc/exist.pem" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth disabled, client: untrusted cert
        # (server cert is self-signed so if used as a client cert it shouldn't be trusted)
        run_reporter none "-t cert/server_cert.pem:cert/server_key.pem" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth disabled, client: trusted cert
        run_reporter none "-t cert/client_cert.pem:cert/client_key.pem" 70
        rlAssertGrep "NOAUTH" server_log
    rlPhaseEnd

    rlPhaseStartTest "Server auth optional"
        # server: auth optional, client: auth disabled
        run_reporter optional "" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth optional, client: invalid cert path
        run_reporter optional "-t /etc/doesnt.pem:/etc/exist.pem" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth optional, client: untrusted cert
        run_reporter optional "-t cert/server_cert.pem:cert/server_key.pem" 70
        rlAssertGrep "NOAUTH" server_log

        # server: auth optional, client: trusted cert
        run_reporter optional "-t cert/client_cert.pem:cert/client_key.pem" 70
        rlAssertGrep "AUTH ureport-reporter-cn" server_log
    rlPhaseEnd

    rlPhaseStartTest "Server auth required"
        # server: auth required, client: auth disabled
        run_reporter required "" 1

        # server: auth required, client: invalid cert path
        run_reporter required "-t /etc/doesnt.pem:/etc/exist.pem" 1

        # server: auth required, client: untrusted cert
        run_reporter required "-t cert/server_cert.pem:cert/server_key.pem" 1

        # server: auth required, client: trusted cert
        run_reporter required "-t cert/client_cert.pem:cert/client_key.pem" 70
        rlAssertGrep "AUTH ureport-reporter-cn" server_log
    rlPhaseEnd

    rlPhaseStartTest "Settings"
        # setting the certificate from configuration file
        CFG=/etc/libreport/plugins/ureport.conf
        mv $CFG conf_backup
        echo "SSLClientAuth = $TmpDir/cert/client_cert.pem:$TmpDir/cert/client_key.pem" > $CFG
        run_reporter required "" 70
        rlAssertGrep "AUTH ureport-reporter-cn" server_log
        mv conf_backup $CFG

        # setting certificate via environment
        uReport_SSLClientAuth="$TmpDir/cert/client_cert.pem:$TmpDir/cert/client_key.pem"
        export uReport_SSLClientAuth
        run_reporter required "" 70
        rlAssertGrep "AUTH ureport-reporter-cn" server_log
        uReport_SSLClientAuth=""
        export -n uReport_SSLClientAuth

        # setting cert implicitly via "rhsm"
        RHSM_DIR=/etc/pki/consumer
        RHSM_CERT=$RHSM_DIR/cert.pem
        RHSM_KEY=$RHSM_DIR/key.pem
        if test -d $RHSM_DIR; then
            EXISTED="yes"
            mv $RHSM_CERT cert_backup
            mv $RHSM_KEY key_backup
        else
            EXISTED="no"
            mkdir -p $RHSM_DIR
        fi
        cp cert/client_cert.pem $RHSM_CERT
        cp cert/client_key.pem $RHSM_KEY

        run_reporter required "-t rhsm" 70
        rlAssertGrep "AUTH ureport-reporter-cn" server_log

        if [ $EXISTED = "yes" ]; then
            mv cert_backup $RHSM_CERT
            mv key_backup $RHSM_KEY
        else
            rm -r $RHSM_DIR
        fi
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "abrt-cli rm $crash_PATH"
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
