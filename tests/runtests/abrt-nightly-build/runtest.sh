#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-nightly-build
#   Description: Clone latest abrt & libreport, build, install
#   Author: Michal Nowak <mnowak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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

PACKAGE="abrt"
TEST="abrt-nightly-build"

MOCKCFG="/etc/mock/fedora-15-x86_64.cfg"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel

        BACKED=0
        if [ -f $MOCKCFG ]; then
            rlFileBackup $MOCKCFG
            BACKED=1
        fi
        cp "mock_fedora-15-x86_64.cfg" $MOCKCFG

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        rlRun "git clone git://git.fedorahosted.org/libreport.git" 0 "Clone libreport.git"
        pushd libreport/
        short_rev=$(git rev-parse --short HEAD)
        rlLog "Git short rev: $short_rev"
        yum-builddep -y --nogpgcheck libreport

        # stupid workaround
        yum reinstall -y "*docbook*"

        # temporary, F16 bump required
        yum install -y json-c-devel

        ./autogen.sh
        rpm --eval '%configure' | sh # ./configure
        rlRun "make srpm"
        rlRun "rpmbuild --rebuild libreport-*.src.rpm" 0 "Build libreport RPMs"
        make
        make install
        mkdir /root/rpmbuild/RPMS/x86_64/
        rlRun "createrepo /root/rpmbuild/RPMS/*/"
        popd # libreport/

        rlRun "git clone git://git.fedorahosted.org/abrt.git" 0 "Clone abrt.git"
        yum-builddep -y --nogpgcheck abrt
        rpmquery btparser-devel > /dev/null || yum install -y btparser-devel --enablerepo="updates-testing"
        /usr/sbin/useradd mock_user --groups mock --create-home
        pushd abrt/
        short_rev=$(git rev-parse --short HEAD)
        rlLog "Git short rev: $short_rev"
    rlPhaseEnd

    rlPhaseStartTest "Build ABRT"
        rlRun "./autogen.sh" 0 "ABRT autogen"
        rlRun "rpm --eval '%configure' | sh" 0 "ABRT configure" # ./configure
        rlRun "make srpm" 0 "ABRT make srpm"
        cp abrt-*.src.rpm ~mock_user/abrt.src.rpm
        rlRun "su - mock_user -c 'mock -v -r fedora-15-x86_64 ~mock_user/abrt.src.rpm'" 0 "Build ABRT in Mock"
        rlRun "yum install -y /var/lib/mock/fedora-15-x86_64/result/abrt* /root/rpmbuild/RPMS/*/libreport*.rpm" 0 "Yum install ABRT & libreport"
        pushd ../libreport/
        rlRun "make install" 0 "Libreport make install"
        popd
        rlRun "make" 0 "ABRT make"
        rlRun "make install" 0 "ABRT make install"
        sed -i 's/OpenGPGCheck.*=.*yes/OpenGPGCheck = no/' /etc/abrt/abrt.conf
        popd # abrt/
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir

        rm -rf $TmpDir
        userdel -r mock_user
        if [ $BACKED -eq 1 ]; then
            rm $MOCKCFG
            rlFileRestore
        fi
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
