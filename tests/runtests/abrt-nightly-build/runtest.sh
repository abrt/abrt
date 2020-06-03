#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-nightly-build
#   Description: Clone latest abrt & libreport, build, install
#   Author: Michal Nowak <mnowak@redhat.com>, Richard Marko <rmarko@redhat.com>
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
. ../aux/lib.sh

PACKAGE="abrt"
TEST="abrt-nightly-build"

if [ -f /etc/fedora-release ]; then
    VERS="fedora-15-x86_64"
    if grep -q 16 '/etc/fedora-release'; then
        VERS="fedora-16-x86_64"
    fi
    if grep -q 17 '/etc/fedora-release'; then
        VERS="fedora-17-x86_64"
    fi
    if grep -q 'Rawhide' '/etc/fedora-release'; then
        VERS="fedora-rawhide-x86_64"
    fi
else
    if grep -q 6 '/etc/redhat-release'; then
        VERS="epel-6-x86_64"
    fi
    if grep -q 7 '/etc/redhat-release'; then
        VERS="epel-7-x86_64"
    fi
fi

MOCKCFG="/etc/mock/${VERS}.cfg"
MOCKRES="/var/lib/mock/${VERS}/result"
TARGETS="btparser libreport abrt"

rlJournalStart
    rlPhaseStartSetup
        rlShowRunningKernel

        BACKED=0
        if [ -f $MOCKCFG ]; then
            rlFileBackup $MOCKCFG
            BACKED=1
        fi
        cp "mock_configs/mock_${VERS}.cfg" $MOCKCFG
        cp local.repo /etc/yum.repos.d/

        TmpDir=$(mktemp -d)
        pushd $TmpDir

        /usr/sbin/useradd mock_user --groups mock --create-home

        mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
        mkdir -p /root/rpmbuild/RPMS/x86_64/
        createrepo /root/rpmbuild/RPMS/*/
    rlPhaseEnd

    for package in $TARGETS; do
        rlPhaseStartTest "Build $package"
            rlRun "git clone https://github.com/abrt/$package.git" 0 "Clone $package.git"

            pushd $package
            rlLog "Git short rev: $(git rev-parse --short HEAD)"

            yum -y install $( grep -oP '^BuildRequires: [^\s]+' $package.spec.in | cut -d: -f2 )

            rlRun "./autogen.sh" 0 "autogen"

            if [ -f "$package-version" ]; then
                rlLog "$package-version: $( cat $package-version )"
            fi

            rlRun "rpm --eval '%configure' | sh" 0 "configure"
            rlRun "make srpm"
            mv -f $package-*.src.rpm ~mock_user/$package.src.rpm
            rlRun "su - mock_user -c 'mock -v -r ${VERS} ~mock_user/$package.src.rpm'" 0 "Build $package in Mock"
            mv -f $MOCKRES/*.src.rpm /root/rpmbuild/SRPMS/
            mv -f $MOCKRES/*.rpm /root/rpmbuild/RPMS/x86_64/
            rlRun "createrepo /root/rpmbuild/RPMS/*/"
            popd
        rlPhaseEnd
    done

    rlPhaseStartTest "Install"
        rlRun "yum -y install /root/rpmbuild/RPMS/*/*.rpm" 0 "Yum install ABRT & libreport"
        rlRun "augtool set /files/etc/abrt/abrt-action-save-package-data.conf/OpenGPGCheck no" 0
    rlPhaseEnd

    rlPhaseStartCleanup
        popd # TmpDir
        rm -rf $TmpDir
        userdel -r mock_user
        rm /etc/yum.repos.d/local.repo
        if [ $BACKED -eq 1 ]; then
            rm $MOCKCFG
            rlFileRestore
        fi
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
