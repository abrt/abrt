# -*- coding: utf-8 -*-
import dbus
import dbus.service
import gobject
from dbus.mainloop.glib import DBusGMainLoop
import gtk
from dbus.exceptions import *
import ABRTExceptions
from abrt_utils import _

CC_NAME = 'com.redhat.abrt'
CC_IFACE = 'com.redhat.abrt'
CC_PATH = '/com/redhat/abrt'

APP_NAME = 'com.redhat.abrt.gui'
APP_PATH = '/com/redhat/abrt/gui'
APP_IFACE = 'com.redhat.abrt.gui'

class DBusManager(gobject.GObject):
    """ Class to provide communication with daemon over dbus """
    # and later with policyKit
    bus = None
    def __init__(self):
        session = None
        # binds the dbus to glib mainloop
        DBusGMainLoop(set_as_default=True)
        class DBusInterface(dbus.service.Object):
            def __init__(self, dbusmanager):
                self.dbusmanager = dbusmanager
                dbus.service.Object.__init__(self, dbus.SessionBus(), APP_PATH)

            @dbus.service.method(dbus_interface=APP_IFACE)
            def show(self):
                self.dbusmanager.emit("show")
        try:
            session = dbus.SessionBus()
        except Exception, e:
            # probably run after "$ su"
            pass

        if session:
            try:
                app_proxy = session.get_object(APP_NAME,APP_PATH)
                app_iface = dbus.Interface(app_proxy, dbus_interface=APP_IFACE)
                # app is running, so make it show it self
                app_iface.show()
                raise ABRTExceptions.IsRunning()
            except DBusException, e:
                # cannot create proxy or call the method => gui is not running
                pass

        gobject.GObject.__init__(self)
        # signal emited when new crash is detected
        gobject.signal_new("crash", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ())
        # signal emited when new analyze is complete
        gobject.signal_new("analyze-complete", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        # signal emited when smth fails
        gobject.signal_new("abrt-error", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        gobject.signal_new("warning", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        # signal emited to update gui with current status
        gobject.signal_new("update", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        # signal emited to show gui if user try to run it again
        gobject.signal_new("show", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ())
        # signal emited to show gui if user try to run it again
        gobject.signal_new("daemon-state-changed", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        gobject.signal_new("report-done", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))

        # export the app dbus interface
        if session:
            session.request_name(APP_NAME)
            iface = DBusInterface(self)

        self.connect_to_daemon()
        self.bus.add_signal_receiver(self.owner_changed_cb, "NameOwnerChanged", dbus_interface="org.freedesktop.DBus")
        # new crash notification
        self.bus.add_signal_receiver(self.crash_cb, "Crash", dbus_interface=CC_IFACE)
        # watch for updates
        self.bus.add_signal_receiver(self.update_cb, "Update", dbus_interface=CC_IFACE)
        # watch for warnings
        self.bus.add_signal_receiver(self.warning_cb, "Warning", dbus_interface=CC_IFACE)
        # watch for job-done signals
        self.bus.add_signal_receiver(self.jobdone_cb, "JobDone", dbus_interface=CC_IFACE)

    # disconnect callback
    def disconnected(self, *args):
        print "disconnect"

    def error_handler_cb(self,error):
        self.emit("abrt-error",error)

    def warning_handler_cb(self,arg):
        self.emit("warning",arg)

    def error_handler(self,arg):
        # used to silently ingore dbus timeouts
        pass

    def dummy(self, *args):
        # dummy function for async method call to workaround the timeout
        pass

    def crash_cb(self,*args):
        #FIXME "got another crash, gui should reload!"
        #for arg in args:
        #    print arg
        #emit a signal
        #print "crash"
        self.emit("crash")

    def update_cb(self, message, job_id=0):
        print "Update >>%s<<" % message
        self.emit("update", message)

    def warning_cb(self, message, job_id=0):
        print "Warning >>%s<<" % message
        self.emit("warning", message)

# Seems to be not needed at all. Not only that, it is actively harmful
# when abrtd is autostarted by dbus-daemon: connect_to_daemon() would install
# duplicate signal handlers!
    def owner_changed_cb(self,name, old_owner, new_owner):
        if name == CC_NAME:
            # No need to connect at once, we can do it when we need it
            # (and this "connect on demand" mode of operation is needed
            # anyway if abrtd is autostarted by dbus: before we call it,
            # there is no abrtd to connect to!)
            if new_owner:
                #self.proxy = self.connect_to_daemon()
                self.emit("daemon-state-changed", "up")
            else:
                #self.proxy = None
                self.emit("daemon-state-changed", "down")

    def connect_to_daemon(self):
        if not self.bus:
            self.bus = dbus.SystemBus()
        if not self.bus:
            raise Exception(_("Can't connect to dbus"))
        # Can't do this: abrtd may be autostarted by dbus-daemon
        #if self.bus.name_has_owner(CC_NAME):
        #    self.proxy = self.bus.get_object(CC_IFACE, CC_PATH, introspect=False)
        #else:
        #    raise Exception(_("Please check if abrt daemon is running."))
        self.proxy = self.bus.get_object(CC_IFACE, CC_PATH, introspect=False)
        if self.proxy:
            self.cc = dbus.Interface(self.proxy, dbus_interface=CC_IFACE)
        else:
            raise Exception(_("Please check if abrt daemon is running."))

    def jobdone_cb(self, dest, uuid):
        # TODO: check that it is indeed OUR job:
        # remember uuid in getReport and compare here
        print "Our job for UUID %s is done." % uuid
        dump = self.cc.GetJobResult(uuid)
        if dump:
            self.emit("analyze-complete", dump)
        else:
            self.emit("abrt-error",_("Daemon did't return valid report info\nDebuginfo is missing?"))

    def report_done(self, result):
        self.emit("report-done", result)

    def getReport(self, UUID):
        try:
            # let's try it async
            # even if it's async it timeouts, so let's try to set the timeout to 60sec
            #self.cc.CreateReport(UUID, reply_handler=self.addJob, error_handler=self.error_handler, timeout=60)
            # we don't need the return value, as the job_id is sent via JobStarted signal
            self.cc.CreateReport(UUID, timeout=60)
        except dbus.exceptions.DBusException, e:
            # One case when it fails is if abrtd exited and needs to be
            # autostarted by dbus again. (This is a first stab
            # at making it work in this case, I want to find a better way)
            self.connect_to_daemon()
            self.cc.CreateReport(UUID, timeout=60)

    def Report(self, report, reporters_settings = None):
        # map < Plguin_name vec <status, message> >
        self.cc.Report(report, reporters_settings, reply_handler=self.report_done, error_handler=self.error_handler_cb, timeout=60)

    def DeleteDebugDump(self,UUID):
        return self.cc.DeleteDebugDump(UUID)

    def getDumps(self):
        row_dict = None
        rows = []
        # FIXME check the arguments
        for row in self.cc.GetCrashInfos():
            row_dict = {}
            for column in row:
                row_dict[column] = row[column]
            rows.append(row_dict);
        return rows

    def getPluginsInfo(self):
        return self.cc.GetPluginsInfo()

    def getPluginSettings(self, plugin_name):
        settings = self.cc.GetPluginSettings(plugin_name)
        #for i in settings.keys():
        #    print i
        return settings

    def registerPlugin(self, plugin_name):
        return self.cc.RegisterPlugin(plugin_name)

    def unRegisterPlugin(self, plugin_name):
        return self.cc.UnRegisterPlugin(plugin_name)

    def setPluginSettings(self, plugin_name, plugin_settings):
        return self.cc.SetPluginSettings(plugin_name, plugin_settings)

    def getSettings(self):
        return self.cc.GetSettings()

    def setSettings(self, settings):
        # FIXME: STUB!!!!
        print "setSettings stub"
        retval = self.cc.SetSettings(self.cc.GetSettings())
        print ">>>", retval
