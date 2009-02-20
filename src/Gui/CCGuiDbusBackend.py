#!/usr/bin/env python

import dbus
import gobject
from dbus.mainloop.glib import DBusGMainLoop
import gtk

CC_IFACE = 'com.redhat.crash_catcher'
CC_PATH = '/com/redhat/crash_catcher'
        

class DBusManager(gobject.GObject):
    """ Class to provide communication with daemon over dbus """
    # and later with policyKit
    def __init__(self):
        gobject.GObject.__init__(self)
        # binds the dbus to glib mainloop
        DBusGMainLoop(set_as_default=True)
        self.proxy = None
        self.connect()
        if self.proxy:
            self.cc = dbus.Interface(self.proxy, dbus_interface=CC_IFACE)
            #intr = dbus.Interface(proxy, dbus_interface='org.freedesktop.DBus.Introspectable')
            self.proxy.connect_to_signal("Crash",self.crash_cb,dbus_interface=CC_IFACE)
        else:
            raise Exception("Proxy object doesn't exist!")

    # disconnect callback
    def disconnected(*args):
        print "disconnect"
    
    def crash_cb(self,*args):
        print "got another crash while in gui!"
        for arg in args:
            print arg
        #emit a signal
        
    def connect(self):
        bus = dbus.SystemBus()
        if not bus:
            raise Exception("Can't connect to dbus")
        try:
            self.proxy = bus.get_object(CC_IFACE, CC_PATH)
        except Exception, e:
            print "Error while creating the proxy"
            print e

    def getDumps(self):
        row_dict = None
        rows = []
        for row in self.cc.GetCrashInfosMap(""):
#            print row
            row_dict = {}
            for column in row:
                row_dict[column] = row[column]
              #  print "%s:%s" % (column, row[column])
            rows.append(row_dict);
        return rows
