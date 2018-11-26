#!/usr/bin/env python3

# argv[1] - problem directory
# argv[2] - element name
# argv[3] - loop counter
# argv[4] - element size in kiB

import dbus
import sys

bus = dbus.SystemBus()
proxy = bus.get_object('org.freedesktop.problems', '/org/freedesktop/problems')
iface = dbus.Interface(proxy, 'org.freedesktop.problems')

try:
    for i in range(int(sys.argv[3])):
        iface.SetElement(sys.argv[1], sys.argv[2], int(sys.argv[4]) * 1024 * "x")
except dbus.exceptions.DBusException as e:
    print('{}: {}'.format(e.get_dbus_name(), e.get_dbus_message()))
