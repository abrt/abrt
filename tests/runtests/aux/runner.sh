#!/bin/bash

. ./aux/lib.sh

load_abrt_conf

if [ $1 ]; then

    # clenaup BEFORE every test
    # stop services
    systemctl stop abrt-oops
    systemctl stop abrt-xorg
    systemctl stop abrtd &> /dev/null

    # cleanup
    echo -n '|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %e' > /var/run/abrt/saved_core_pattern

    if pidof abrtd; then
        killall -9 abrtd
        rm -f /var/run/abrt/abrtd.pid
    fi

    prepare

    if [ "${REINSTALL_BEFORE_EACH_TEST}" = "1" ]; then
        echo 'REINSTALL_BEFORE_EACH_TEST set'

        # Do not remove libreport-filesystem because dnf package and a few
        # other system packages requires it.
        dnf -y remove abrt\* libreport\* --exclude libreport-filesystem

        rm -rf /etc/abrt/

        # All libreport and abrt packages should be removed so any
        # configuration file that does not belong to an rpm package was created
        # by a test and must be removed.
        for CONF in `find /etc/libreport/`; do
            rpm -qf $CONF > /dev/null || rm --preserve-root -r --force $CONF
        done

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

    rlServiceStart abrtd abrt-journal-core abrt-oops

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
        /usr/bin/time -v -o $tmpfile /usr/bin/timeout $TEST_TIMEOUT ./$(basename $1)
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

