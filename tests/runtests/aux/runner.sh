#!/bin/bash

. ./aux/lib.sh

load_abrt_conf

if [ $1 ]; then

    # clenaup BEFORE every test
    # stop services
    service abrt-oops stop
    service abrt-xorg stop
    service abrtd stop &> /dev/null

    # cleanup
    echo 'core' > /var/run/abrt/saved_core_pattern

    if [ -x /usr/sbin/abrt-install-ccpp-hook ]; then
        /usr/sbin/abrt-install-ccpp-hook uninstall
    fi

    if pidof abrtd; then
        killall -9 abrtd
        rm -f /var/run/abrt/abrtd.pid
    fi

    prepare

    if [ "${REINSTALL_BEFORE_EACH_TEST}" = "1" ]; then
        echo 'REINSTALL_BEFORE_EACH_TEST set'

        dnf -y remove abrt\* libreport\*

        rm -rf /etc/abrt/
        rm -rf /etc/libreport/

        dnf -y install $PACKAGES
    fi

    if [ "${RESTORE_CONFIGS_BEFORE_EACH_TEST}" = "1" ]; then
        echo 'RESTORE_CONFIGS_BEFORE_EACH_TEST set'

        if [ -d /tmp/abrt-config ]; then
            rm -rf /etc/abrt/
            rm -rf /etc/libreport/

            cp -a /tmp/abrt-config/abrt /etc/abrt
            cp -a /tmp/abrt-config/libreport /etc/libreport
        else
            echo 'Nothing to restore'
        fi
    fi

    if [ "${CLEAN_SPOOL_BEFORE_EACH_TEST}" = "1" ]; then
        rm -rf $ABRT_CONF_DUMP_LOCATION/*
    fi

    if [ "${DUMP_PACKAGE_VERSIONS}" = "1" ]; then
        rpm -q $PACKAGES
    fi

    service abrtd start
    service abrt-ccpp start
    service abrt-oops start

    # test delay
    if [ "${DELAY+set}" = "set" ]; then
        echo "sleeping for $DELAY seconds before running the test"
        echo "(to avoid crashes not being dumped due to time limits)"
        sleep $DELAY
    fi

    # run test
    pushd $(dirname $1)
    echo ":: TEST START MARK ::"
    if [ -x /usr/bin/time ]; then
        tmpfile=$( mktemp )
        /usr/bin/time -v -o $tmpfile ./$(basename $1)
        cat $tmpfile
        rm $tmpfile
    else
        ./$(basename $1)
    fi
    echo ":: TEST END MARK ::"
    popd

    exit 0
else
    echo "Provide test name"
    exit 1
fi

