# -*- coding: utf-8 -*-
import dbus
import gobject
from dbus.mainloop.glib import DBusGMainLoop
import gtk

CC_IFACE = 'com.redhat.abrt'
CC_PATH = '/com/redhat/abrt'
        

class DBusManager(gobject.GObject):
    """ Class to provide communication with daemon over dbus """
    # and later with policyKit
    def __init__(self):
        gobject.GObject.__init__(self)
        # signal emited when new crash is detected
        gobject.signal_new ("crash", self ,gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,())
        # signal emited when new analyze is complete
        gobject.signal_new ("analyze-complete", self ,gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        # signal emited when smth fails
        gobject.signal_new ("error", self ,gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        # binds the dbus to glib mainloop
        DBusGMainLoop(set_as_default=True)
        self.proxy = None
        self.connect_to_daemon()
        if self.proxy:
            self.cc = dbus.Interface(self.proxy, dbus_interface=CC_IFACE)
            #intr = dbus.Interface(proxy, dbus_interface='org.freedesktop.DBus.Introspectable')
            # new crash notify
            self.proxy.connect_to_signal("Crash",self.crash_cb,dbus_interface=CC_IFACE)
            # BT extracting complete
            self.acconnection = self.proxy.connect_to_signal("AnalyzeComplete",self.analyze_complete_cb,dbus_interface=CC_IFACE)
            # Catch Errors
            self.acconnection = self.proxy.connect_to_signal("Error",self.error_handler_cb,dbus_interface=CC_IFACE)
        else:
            raise Exception("Proxy object doesn't exist!")

    # disconnect callback
    def disconnected(*args):
        print "disconnect"
    
    def error_handler_cb(self,arg):
        self.emit("error",arg)
    
    def error_handler(self,arg):
        # used to silently ingore dbus timeouts
        pass
    
    def dummy(*args):
        # dummy function for async method call to workaround the timeout
        pass
    
    def crash_cb(self,*args):
        #FIXME "got another crash, gui should reload!"
        #for arg in args:
        #    print arg
        #emit a signal
        #print "crash"
        self.emit("crash")
        
    def analyze_complete_cb(self,dump):
        #for arg in args:
        #    print "Analyze complete for: %s" % arg
        # emit signal to let clients know that analyze has been completed
        # FIXME - rewrite with CCReport class
        self.emit("analyze-complete", dump)
    
    def connect_to_daemon(self):
        bus = dbus.SystemBus()
        if not bus:
            raise Exception("Can't connect to dbus")
        try:
            self.proxy = bus.get_object(CC_IFACE, CC_PATH)
        except Exception, e:
            raise Exception(e.message + "\nPlease check if abrt daemon is running.")

    def getReport(self, UUID):
        try:
            # let's try it async
            # even if it's async it timeouts, so let's try to set the timeout to 60sec
            self.cc.CreateReport(UUID, reply_handler=self.dummy, error_handler=self.error_handler, timeout=60)
        except dbus.exceptions.DBusException, e:
            raise Exception(e.message)
    
    def Report(self,report):
        # FIXME async
        return self.cc.Report(report)
    
    def DeleteDebugDump(self,UUID):
        return self.cc.DeleteDebugDump(UUID)
    
    def getDumps(self):
        row_dict = None
        rows = []
        # FIXME check the arguments
        for row in self.cc.GetCrashInfos(""):
            row_dict = {}
            for column in row:
                row_dict[column] = row[column]
            rows.append(row_dict);
        return rows
