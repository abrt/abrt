#!/bin/bash

set -e
set -x

dnf install -y \
    abrt \
    abrt-tui \
    abrt-addon-ccpp \
    python3-abrt-addon \
    libreport-plugin-logger \
    will-crash \
    augeas \
    pkgconf-pkg-config \
    expect \
    gcc \
    rpm-build \
    rpm-sign

# FIXME: Test dependencies should be listed in their corresponding ".fmf" files;
#        However, that doesn't work for me for some reason (still learning tmt:))

systemctl start abrtd abrt-journal-core

# "wait_for_hooks" step waits for the "/tmp/abrt-done" file to appear
cat > /etc/libreport/events.d/test_event.conf << _EOF_
EVENT=notify
        touch /tmp/abrt-done
EVENT=notify-dup
        touch /tmp/abrt-done
_EOF_
