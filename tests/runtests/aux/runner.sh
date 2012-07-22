#!/bin/bash

if [ $1 ]; then

    # clenaup BEFORE every test
    # stop services
    service abrt-oops stop
    service abrt-xorg stop
    service abrtd stop

    # cleanup
    if [ -x /usr/sbin/abrt-install-ccpp-hook ]; then
        /usr/sbin/abrt-install-ccpp-hook uninstall
    fi

    if [ -f /var/run/abrt/saved_core_pattern ]; then
        rm -f /var/run/abrt/saved_core_pattern
    fi

    rm -f /var/spool/abrt/last-ccpp
    yum remove abrt\* libreport\* -y;

    rm -rf /etc/abrt/
    rm -rf /etc/libreport/
    rm -rf /var/spool/abrt/*

    PACKAGES="abrt-desktop \
    abrt-cli \
    libreport-plugin-reportuploader \
    libreport-plugin-mailx";

    yum install $PACKAGES -y;

    service abrtd restart
    service abrt-ccpp restart
    service abrt-oops restart

    # test delay
    if [ "${DELAY+set}" = "set" ]; then
        echo "sleeping for $DELAY seconds before running the test"
        echo "(to avoid crashes not being dumped due to time limits)"
        sleep $DELAY
    fi

    # run test
    pushd $(dirname $1)
    echo ":: TEST START MARK ::"
    ./$(basename $1)
    echo ":: TEST END MARK ::"
    popd

    exit 0
else
    echo "Provide test name"
    exit 1
fi

