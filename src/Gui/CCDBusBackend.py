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
    uniq_name = None
    def __init__(self):
        self.pending_jobs = []
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
        gobject.signal_new ("crash", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,())
        # signal emited when new analyze is complete
        gobject.signal_new ("analyze-complete", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        # signal emited when smth fails
        gobject.signal_new ("abrt-error", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        gobject.signal_new ("warning", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        # signal emited to update gui with current status
        gobject.signal_new ("update", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,gobject.TYPE_PYOBJECT))
        # signal emited to show gui if user try to run it again
        gobject.signal_new ("show", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,())
        # signal emited to show gui if user try to run it again
        gobject.signal_new ("daemon-state-changed", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))
        gobject.signal_new ("report-done", self, gobject.SIGNAL_RUN_FIRST,gobject.TYPE_NONE,(gobject.TYPE_PYOBJECT,))

        # export the app dbus interface
        if session:
            session.request_name(APP_NAME)
            iface = DBusInterface(self)

        self.connect_to_daemon()

    # disconnect callback
    def disconnected(*args):
        print "disconnect"

    def error_handler_cb(self,error):
        self.emit("abrt-error",error)

    def warning_handler_cb(self,arg):
        self.emit("warning",arg)

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

    def update_cb(self, message, job_id=0):
        print "Update >>%s<< for job: %s" % (message, job_id)
        # FIXME: use dest instead of 0 once we implement it in daemon
        #if self.uniq_name == dest:
        if job_id == 0 or job_id in self.pending_jobs:
            self.emit("update", message, job_id)

    def warning_cb(self, message, job_id=0):
        print "Warning >>%s<< for job: %s" % (message, job_id)
        # FIXME: use dest instead of 0 once we implement it in daemon
        #if self.uniq_name == dest:
        if job_id == 0 or job_id in self.pending_jobs:
            self.emit("warning", message)

    def analyze_complete_cb(self,dump):
        #for arg in args:
        #    print "Analyze complete for: %s" % arg
        # emit signal to let clients know that analyze has been completed
        # FIXME - rewrite with CCReport class
    #    self.emit("analyze-complete", dump)
        pass

    def owner_changed_cb(self,name, old_owner, new_owner):
        if(name == CC_NAME and new_owner):
            self.proxy = self.connect_to_daemon()
            self.emit("daemon-state-changed", "up")
        if(name == CC_NAME and not(new_owner)):
            self.proxy = None
            self.emit("daemon-state-changed", "down")


    def connect_to_daemon(self):
        if not self.bus:
            self.bus = dbus.SystemBus()
            self.bus.add_signal_receiver(self.owner_changed_cb,"NameOwnerChanged", dbus_interface="org.freedesktop.DBus")
        self.uniq_name = self.bus.get_unique_name()
        if not self.bus:
            raise Exception(_("Can't connect to dbus"))
        if self.bus.name_has_owner(CC_NAME):
            self.proxy = self.bus.get_object(CC_IFACE, CC_PATH)
        else:
            raise Exception(_("Please check if abrt daemon is running."))

        if self.proxy:
            self.cc = dbus.Interface(self.proxy, dbus_interface=CC_IFACE)
            #intr = dbus.Interface(proxy, dbus_interface='org.freedesktop.DBus.Introspectable')
            # new crash notify
            self.proxy.connect_to_signal("Crash",self.crash_cb,dbus_interface=CC_IFACE)
            # BT extracting complete
            self.acconnection = self.proxy.connect_to_signal("AnalyzeComplete",self.analyze_complete_cb,dbus_interface=CC_IFACE)
            # Catch Errors
            self.acconnection = self.proxy.connect_to_signal("Error",self.error_handler_cb,dbus_interface=CC_IFACE)
            # watch for updates
            self.acconnection = self.proxy.connect_to_signal("Update",self.update_cb,dbus_interface=CC_IFACE)
            # watch for warnings
            self.acconnection = self.proxy.connect_to_signal("Warning",self.warning_cb,dbus_interface=CC_IFACE)
            # watch for job-done signals
            self.acconnection = self.proxy.connect_to_signal("JobDone",self.jobdone_cb,dbus_interface=CC_IFACE)
            self.acconnection = self.proxy.connect_to_signal("JobStarted",self.jobstarted_cb,dbus_interface=CC_IFACE)
        else:
            raise Exception(_("Please check if abrt daemon is running."))

    def addJob(self, job_id):
        self.pending_jobs.append(job_id)

    def jobstarted_cb(self, dest, job_id):
        # the job belongs to this client
        if self.uniq_name == dest:
            print "Started our job: %s" % job_id
            self.addJob(job_id)

    def jobdone_cb(self, dest, job_id):
        if self.uniq_name == dest:
            print "Our job: %s is done." % job_id
            dump = self.cc.GetJobResult(job_id)
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
            raise Exception(e)

    def Report(self,report):
        # map < Plguin_name vec <status, message> >
        self.cc.Report(report, reply_handler=self.report_done, error_handler=self.error_handler_cb, timeout=60)

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

