#!/bin/bash

export > $OUTPUT_ROOT/pre/envs.log
cp /var/log/messages $OUTPUT_ROOT/pre/messages
dmesg -c > $OUTPUT_ROOT/pre/dmesg_pre

dnf clean metadata

#dnf install -y beakerlib dejagnu time createrepo mock expect
if [ "${REINSTALL_PRE}" = "1" ]; then
    echo 'REINSTALL_PRE set'

    rpm -qa abrt\* libreport\* python3-abrt\* satyr\* will-crash\* \
        | xargs rpm -e --nodeps

    # If there is still something installed that is in PACKAGES, this will
    # leave those packages broken, and you will find yourself scratching your
    # head, as “dnf install” does not reinstall.
    rm -rf /etc/abrt/
    rm -rf /etc/libreport/

    dnf -y install $PACKAGES
fi

cat > /etc/libreport/events.d/test_event.conf << _EOF_
EVENT=notify
        echo "ABRT tests - EVENT=notify - $DUMP_DIR"
        touch /tmp/abrt-done
        true
EVENT=notify-dup
        echo "ABRT tests - EVENT=notify-dup - $DUMP_DIR"
        touch /tmp/abrt-done
        true
_EOF_

augtool set /files/etc/abrt/abrt.conf/MaxCrashReportsSize 1000

if [ "${DISABLE_GPGCHECK}" = "1" ]; then
    augtool set /files/etc/abrt/abrt-action-save-package-data.conf/OpenGPGCheck no
fi

if [ "${DISABLE_AUTOREPORTING}" = "1" ]; then
    which abrt-auto-reporting && abrt-auto-reporting disabled
fi

if [ "${STORE_CONFIGS}" = "1" ]; then
    echo 'STORE_CONFIGS set'
    rm -rf /tmp/abrt-config/
    mkdir /tmp/abrt-config

    cp -a /etc/abrt /tmp/abrt-config/abrt
    cp -a /etc/libreport /tmp/abrt-config
fi

if [ "${UPDATE_PACKAGES}" = "1" ]; then
    echo 'UPDATE_PACKAGES set'
    dnf -y update $PACKAGES
fi

if [ "${UPDATE_SYSTEM}" = "1" ]; then
    echo 'UPDATE_SYSTEM set'
    dnf -y update
fi

if [ "${DISABLE_NOAUDIT}" = "1" ]; then
    # turn off noaudit
    semodule -DB
fi
