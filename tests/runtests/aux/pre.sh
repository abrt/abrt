#!/bin/bash

export > $OUTPUT_ROOT/pre/envs.log
cp /var/log/messages $OUTPUT_ROOT/pre/messages
dmesg -c > $OUTPUT_ROOT/pre/dmesg_pre

yum clean metadata

#yum install -y beakerlib dejagnu time createrepo mock expect
if [ "${REINSTALL_PRE}" = "1" ]; then
    echo 'REINSTALL_PRE set'

    yum -y remove abrt\* libreport\*

    rm -rf /etc/abrt/
    rm -rf /etc/libreport/

    yum -y install $PACKAGES
fi

cat > /etc/libreport/events.d/test_event.conf << _EOF_
EVENT=notify
        touch /tmp/abrt-done
_EOF_

if [ "${DISABLE_GPGCHECK}" = "1" ]; then
    sed -i 's/OpenGPGCheck.*=.*yes/OpenGPGCheck = no/' \
        /etc/abrt/abrt-action-save-package-data.conf
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
    yum -y update $PACKAGES
fi

if [ "${UPDATE_SYSTEM}" = "1" ]; then
    echo 'UPDATE_SYSTEM set'
    yum -y update
fi

if [ "${DISABLE_NOAUDIT}" = "1" ]; then
    # turn off noaudit
    semodule -DB
fi
