#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of docker-essentials
#   Description: Verify detection of various problems inside docker containers
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2016 Red Hat, Inc. All rights reserved.
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

TEST="docker-essentials"
PACKAGE="abrt"

DOCKER_IMAGE="fedora"
DOCKER_NAME=abrt_integration_test

rlJournalStart
    rlPhaseStartSetup
        prepare
        check_prior_crashes

        TmpDir=$(mktemp -d)

        systemctl status docker > /dev/null || rlDie "Docker is not runnig"
        docker inspect $DOCKER_IMAGE || rlDie "Fedora image is not downloaded"

        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "Kill sleep in fedora image"
        rlRun "docker run --name $DOCKER_NAME $DOCKER_IMAGE /usr/bin/bash -c \"timeout -s ABRT 1 sleep 10\"" 124
        rlRun "docker inspect $DOCKER_NAME > docker_inspect"
        rlRun "DOCKER_ID=$(docker ps -a -f name=$DOCKER_NAME --format \"{{.ID}}\")"

        wait_for_hooks
        get_crash_path

        ls $crash_PATH > crash_dir_ls
        check_dump_dir_attributes $crash_PATH

        rlAssertEquals    "Docker in container file" "_$(cat $crash_PATH/container)"        "_docker"
        rlAssertEquals    "Found correct image"      "_$(cat $crash_PATH/container_image)"  "_$DOCKER_IMAGE"
        rlAssertEquals    "Grabbed correct ID"       "_$(cat $crash_PATH/container_id)"     "_$DOCKER_ID"
        rlAssertNotDiffer                                    $crash_PATH/docker_inspect      docker_inspect
        rlAssertGrep      "docker"                          "$crash_PATH/container_cmdline"

        rlRun "docker rm abrt_integration_test"
    rlPhaseEnd

    rlPhaseStartCleanup
        abrt-cli rm $crash_PATH
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
