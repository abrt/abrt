#!/bin/bash

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
    if ! pidof abrtd 2>&1 > /dev/null; then
        if [ -x /usr/sbin/abrtd ]; then
            if [ -x /bin/systemctl ]; then
                /bin/systemctl restart dbus.service
            else
                /usr/sbin/service dbus restart
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
            /usr/bin/abrt-dump-oops -d /var/spool/abrt -rwx /var/log/messages &
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

    # cleanup
    if [ -x /usr/sbin/abrt-install-ccpp-hook ]; then
        abrt-install-ccpp-hook uninstall
    fi
    pidof abrtd && killall abrtd
    pidof abrt-dump-oops && killall abrt-dump-oops

    exit 0
else
    echo "Provide test name"
    exit 1
fi

