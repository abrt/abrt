#!/bin/bash

echo "Disabling selinux"
setenforce 0

if [ $1 ]; then
    # core pattern
    if ! cat /proc/sys/kernel/core_pattern | grep -q abrt; then
        if [ -x /usr/sbin/abrt-install-ccpp-hook ]; then
            /usr/sbin/abrt-install-ccpp-hook install
            echo "core_pattern: $(cat /proc/sys/kernel/core_pattern)"
        else
            echo "core_pattern: abrt-install-ccpp-hook not present, skipping"
        fi
    fi

    # abrtd
    dbus_service_name=messagebus
    if ! pidof abrtd 2>&1 > /dev/null; then
        if [ -x /usr/sbin/abrtd ]; then
            if [ -x /bin/systemctl ]; then
                /bin/systemctl restart $dbus_service_name.service
            else
                if [ -x /usr/sbin/service ]; then
                    /usr/sbin/service $dbus_service_name restart
                else
                    /sbin/service $dbus_service_name restart
                fi
            fi
            /usr/sbin/abrtd -s
            echo "abrtd PID: $(pidof abrtd)"
        else
            echo "abrtd: not present, skipping"
        fi
    fi

    # abrt-dump-oops
    if ! pidof abrt-dump-oops 2>&1 > /dev/null; then
        if [ -x /usr/bin/abrt-dump-oops ]; then
            /usr/bin/abrt-dump-oops -d /var/spool/abrt -x /var/log/messages &
            echo "abrt-dump-oops PID: $(pidof abrt-dump-oops)"
        else
            echo "abrt-dump-oops: not present, skipping"
        fi
    fi

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

    exit 0
else
    echo "Provide test name"
    exit 1
fi

