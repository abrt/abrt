#!/bin/bash
# vim: dict+=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of abrt-action-install-debuginfo
#   Description: Test for CVE-2015-3159 (abrt missing process environment sanitizaton in)
#   Author: Martin Kyral <mkyral@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2015 Red Hat, Inc.
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

# Include Beaker environment
. /usr/share/beakerlib/beakerlib.sh || exit 1
. ../aux/lib.sh || exit 1

TEST="abrt-action-install-debuginfo"

PACKAGES=${PACKAGES:-"abrt"}

rlJournalStart
    rlPhaseStartSetup
        load_abrt_conf
        [ -n "$ABRT_CONF_DUMP_LOCATION" ] || rlDie "Missing DumpLocation"

        check_prior_crashes

        rlAssertRpm --all

        rlServiceStart abrtd
        rlServiceStart abrt-ccpp

        rlRun "TmpDir=\$(mktemp -d)" 0 "Creating tmp directory"
        rlRun "pushd $TmpDir"

        rlRun "adduser testuser"

        rlRun "su testuser -c will_segfault" 139
        wait_for_hooks
        get_crash_path

        pushd $crash_PATH
        rlRun "abrt-action-analyze-core --core=coredump -o build_ids" 0 "Prepare build_ids"
        rlRun "chmod 640 $crash_PATH/build_ids" 0 "Correct permissions"

        popd
        rlRun "mv /usr/lib/debug/usr $TmpDir" 0,1 "Make sure there are no system debuginfos"
        rlRun "rm -rf /var/cache/abrt-di/usr" 0,1 "Make sure there are no debuginfos in ABRT cache"
    rlPhaseEnd

    rlPhaseStartTest "Sanitization of command line arguments"
        TESTPERFORMED=0
        for BUILDID in $(cat $crash_PATH/build_ids) ; do
            rm -rf $ABRT_CONF_DUMP_LOCATION/usr &>/dev/null
            rlRun "echo $BUILDID | su testuser -c \"(umask 0; /usr/libexec/abrt-action-install-debuginfo-to-abrt-cache --id=- --tmpdi $ABRT_CONF_DUMP_LOCATION/usr --ca $ABRT_CONF_DUMP_LOCATION -y)\""
            ls -l $ABRT_CONF_DUMP_LOCATION
            rlAssertNotExists $ABRT_CONF_DUMP_LOCATION/usr
            rlRun "ls -l $ABRT_CONF_DUMP_LOCATION | grep usr | grep rwxrwxrwx" 1 "The spoofed dump dir has not a+rwx"
            rm -rf $ABRT_CONF_DUMP_LOCATION/usr &>/dev/null
            TESTPERFORMED=1
        done
        [ $TESTPERFORMED -eq 0 ] && rlWarn "No actual testing happened"
    rlPhaseEnd

    rlPhaseStartTest "The wrapper creates a new temporary directory"
        ABRT_BINARY=/usr/bin/abrt-action-install-debuginfo
        ABRT_BINARY_BACKUP=$ABRT_BINARY"_wrapped"

        mv $ABRT_BINARY $ABRT_BINARY_BACKUP

cat > $ABRT_BINARY <<EOF
#!/usr/bin/bash
function test_log
{
    echo "abrt-test-wrapper:" \$@
}

test_log "Executing abrt-action-install-debuginfo test wrapper"

TMPDIRECTORY=""
argn=1
while [ \$argn -lt \$# ]
do
    case \${!argn} in
        "--tmpdir")
            test -n "\$TMPDIRECTORY" && {
                test_log "The tmp directory is already configured"
                exit 7
            }

            argn=\$((argn+1))

            TMPDIRECTORY=\${!argn}
        ;;
    esac

    argn=\$((argn+1))
done

test -z "\$TMPDIRECTORY" && {
    test_log "The wrapper did not pass --tmpdir argument"
    exit 17
}

echo "\$TMPDIRECTORY" | grep -q "^/var/tmp/abrt-tmp-debuginfo\.......$" || {
    test_log "The passed tmp directory path does not match the expected pattern"
    exit 27
}

test -d "\$TMPDIRECTORY" || {
    test_log "The passed tmp directory does not exist"
    exit 37
}

test -n "\$(ls -A -- \$TMPDIRECTORY)" && {
    test_log "The passed tmp directory is not empty"
    exit 47
}

STAT=\$(stat --printf "%a %G %U" -- \$TMPDIRECTORY)
test "700 abrt abrt" = "\$STAT" || {
    test_log "The passed tmp directory is not of '700 abrt abrt' (\$STAT)"
    exit 57
}

test_log "The tmp directory looks OK. Going to run the wrapped executable."

$ABRT_BINARY_BACKUP \$@ || {
    test_log "The abrt-action-install-debuginfo command has failed"
    exit 67
}

test -d "\$TMPDIRECTORY" && {
    test_log "The tmp directory has not been removed. Was the directory ignored?"
    exit 77
}

test_log "The tmp directory has been removed, so the directory wasn't ignored."
exit 0
EOF
        chmod +rx $ABRT_BINARY
        ls -al $ABRT_BINARY $ABRT_BINARY_BACKUP

        rlRun "/usr/libexec/abrt-action-install-debuginfo-to-abrt-cache --ids=$crash_PATH/build_ids -y" 0 "Temporary directory was used"

        mv $ABRT_BINARY_BACKUP $ABRT_BINARY
    rlPhaseEnd

    rlPhaseStartTest "The wrapper does not read inaccessible files"
        rlRun "chown -R testuser:abrt $crash_PATH"
        ls -l $crash_PATH/build_ids
        rlRun "su testuser -c \"/usr/libexec/abrt-action-install-debuginfo-to-abrt-cache --ids=$crash_PATH/build_ids -y\"" 0 "The downloader works"
        rlAssertExists "/var/cache/abrt-di/usr"
        ls -ld /var/cache/abrt-di/usr
        rlRun "ls -ld /var/cache/abrt-di/usr | grep -e \"^drwxr-xr-x\. [0-9]\+ abrt abrt \""
        rlRun "rm -rf /var/cache/abrt-di/usr" 0,1 "Clean ABRT cache"

        rlRun "chown abrt:abrt $crash_PATH/build_ids"
        ls -l $crash_PATH/build_ids
        rlRun "su testuser -c \"cat $crash_PATH/build_ids\"" 1
        rlRun "su testuser -c \"/usr/libexec/abrt-action-install-debuginfo-to-abrt-cache --ids=$crash_PATH/build_ids -y\"" 1 "Refuses to open inaccessible file"
        rlAssertNotExists "/var/cache/abrt-di/usr"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlServiceRestore abrtd
        rlRun "rm -rf /var/cache/abrt-di/usr" 0,1 "Clean ABRT cache"
        rlRun "rm -r /usr/lib/debug/usr" 0,1
        rlRun "mv $TmpDir/usr /usr/lib/debug" 0,1
        rlRun "abrt-cli rm $crash_PATH"
        rlRun "userdel -r -f testuser"
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
rlJournalPrintText
rlJournalEnd
