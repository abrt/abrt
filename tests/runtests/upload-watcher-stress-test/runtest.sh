#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of upload-watcher-stress-test
#   Description: Test upload watcher performance
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
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

TEST="upload-watcher-stress-test"
PACKAGE="abrt"

flood()
{
    touch /tmp/flood.$1$2
    for e in `seq $1 $2`; do
        for i in `seq $((e * 1000)) $((e * 1000 + 999))`; do
            echo "$i.tar.gz" > $WATCHED_DIR/$i.tar.gz
        done
        sleep 1
    done
    rm /tmp/flood.$1$2
}

rlJournalStart
    rlPhaseStartSetup
        WATCHED_DIR=$PWD/watched
        export WATCHED_DIR
        mkdir -p $WATCHED_DIR

        # the upload watcher is not installed by default, but we need it for this test
        upload_watch_pkg="abrt-addon-upload-watch"
        rlRun "rpm -q $upload_watch_pkg >/dev/null || dnf install $upload_watch_pkg -y"
        # Adding $PWD to PATH in order to override abrt-handle-upload
        # by a local script
        # Use 60 workers and in the worst case 1GiB for cache
        ERR_LOG="err.log"
        PATH="$PWD:$PATH:/usr/sbin" abrt-upload-watch -w 60 -c 1024 -v $WATCHED_DIR > out.log 2>$ERR_LOG &
        PID_OF_WATCH=$!
    rlPhaseEnd

    rlPhaseStartTest "handle upload"
        echo "Copying samples to the watched directory"

        flood 0 9 &
        flood 10 19 &

        while test -f "/tmp/flood.09";do sleep 1; done
        while test -f "/tmp/flood.1019";do sleep 1; done

        echo "Synchronization waiting ..."

        # Wait while out.log is growing ..."
        OLD_SIZE=0
        CYCLE=0
        WAS_SAME=0
        while : ; do
            kill -s USR1 $PID_OF_WATCH
            tail -n 10 $ERR_LOG | grep "0 archives to process, 0 active workers"
            if [ x"$?" == x"0" ]; then
                # we can't use size of the log, the output is buffered!
                NEW_SIZE=$(ls -l $WATCHED_DIR 2>/dev/null | wc -l)
                if [ $((NEW_SIZE - OLD_SIZE)) -eq 0 ]; then
                    WAS_SAME=$((WAS_SAME+1))
                fi
                if [ $WAS_SAME -gt 5 ]; then # it has to be 5 times the same
                    break
                fi
            fi

            OLD_SIZE=$NEW_SIZE

            CYCLE=$((CYCLE + 1))


            test $CYCLE -gt 100 && break

            sleep 2
        done

        kill $PID_OF_WATCH

        echo "Checking results"
        #
        # $ ls -l $WATCHED_DIR
        # total 0
        # $
        #
        rlAssertEquals "The watched directory is empty" "_1" "_$(ls -l $WATCHED_DIR | wc -l)"
    rlPhaseEnd

    rlPhaseStartCleanup
        rm -rf $WATCHED_DIR
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
