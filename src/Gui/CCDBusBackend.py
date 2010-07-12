# -*- coding: utf-8 -*-
import time  # for sleep()
import gobject
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from dbus.exceptions import *
import ABRTExceptions
from abrt_utils import _, log, log1, log2

ABRTD_DBUS_NAME = 'com.redhat.abrt'
ABRTD_DBUS_PATH = '/com/redhat/abrt'
ABRTD_DBUS_IFACE = 'com.redhat.abrt'

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
        except Exception:
            # probably run after "$ su"
            pass

        if session:
            try:
                app_proxy = session.get_object(APP_NAME, APP_PATH)
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
        gobject.signal_new("daemon-state-changed", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))
        gobject.signal_new("report-done", self, gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,))

        # export the app dbus interface
        if session:
            session.request_name(APP_NAME)
            iface = DBusInterface(self)

        self.bus = dbus.SystemBus()
        if not self.bus:
            raise Exception(_("Cannot connect to system dbus."))
        self.bus.add_signal_receiver(self.owner_changed_cb, "NameOwnerChanged", dbus_interface="org.freedesktop.DBus")
        # new crash notification
        self.bus.add_signal_receiver(self.crash_cb, "Crash", dbus_interface=ABRTD_DBUS_IFACE)
        # watch for updates
        self.bus.add_signal_receiver(self.update_cb, "Update", dbus_interface=ABRTD_DBUS_IFACE)
        # watch for warnings
        self.bus.add_signal_receiver(self.warning_cb, "Warning", dbus_interface=ABRTD_DBUS_IFACE)
        # watch for job-done signals
        self.bus.add_signal_receiver(self.jobdone_cb, "JobDone", dbus_interface=ABRTD_DBUS_IFACE)

    # We use this function instead of caching and reusing of
    # dbus.Interface(proxy, dbus_interface=ABRTD_DBUS_IFACE) because we want
    # to restart abrtd in this scenario:
    # (1) abrt-gui was run
    # (2) user generated the report, then left for coffee break
    # (3) abrtd exited on inactivity timeout
    # (4) user returned and wants to submit the report
    # for (4) to restart abrtd, we must recreate proxy and daemon
    def daemon(self):
        if not self.bus:
            self.bus = dbus.SystemBus()
            if not self.bus:
                raise Exception(_("Cannot connect to system dbus."))
        # Autostart hack
        # Theoretically, this is not needed, the first dbus call
        # should autostart daemon if needed. In practice,
        # without this code autostart is not reliable.
        # Update: fixed the problem on daemon side,
        # but retaining this code for now. More stability won't hurt us...
        try:
            (always_true, rc) = self.bus.start_service_by_name(ABRTD_DBUS_NAME, flags=0)
            # rc is either self.bus.START_REPLY_SUCCESS or self.bus.START_REPLY_ALREADY_RUNNING
            if rc == self.bus.START_REPLY_SUCCESS:
                # Better solution may be to have daemon emit a signal and wait for it
                log1("dbus auto-started abrt daemon, giving it 1 sec to initialize")
                time.sleep(1)
        except DBusException:
            raise Exception("abrt daemon is not running, and DBus can't start it")
        # End of autostart hack
        try:
            # follow_name_owner_changes=True: switch to new daemon if daemon is restarted
            proxy = self.bus.get_object(ABRTD_DBUS_NAME, ABRTD_DBUS_PATH, introspect=False, follow_name_owner_changes=True)
        except DBusException:
            raise Exception("Can't connect to abrt daemon")
        if not proxy:
            raise Exception(_("Please check if the abrt daemon is running."))
        daemon = dbus.Interface(proxy, dbus_interface=ABRTD_DBUS_IFACE)
        if not daemon:
            raise Exception(_("Please check if the abrt daemon is running."))
        return daemon

#    # disconnect callback
#    def disconnected(self, *args):
#        print "disconnect"

    def error_handler_cb(self, error):
        self.emit("abrt-error", error)

    def warning_handler_cb(self, arg):
        self.emit("warning", arg)

    def error_handler(self, arg):
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

    def update_cb(self, message):
        log1("Update:%s", message)
        self.emit("update", message)

    def warning_cb(self, message):
        log1("Warning:%s", message)
        self.emit("warning", message)

    def owner_changed_cb(self, name, old_owner, new_owner):
        if name == ABRTD_DBUS_NAME:
            if new_owner:
                self.emit("daemon-state-changed", "up")
            else:
                self.emit("daemon-state-changed", "down")

    def jobdone_cb(self):
        log1("Our job for crash_id %s is done", self.job_crash_id)
        report = self.daemon().CreateReport(self.job_crash_id)
        if report:
            self.emit("analyze-complete", report)
        else:
            # FIXME: BUG: BarWindow remains. (how2reproduce: delete "component" in a dump dir and try to report it)
            self.emit("abrt-error", _("Daemon did not return a valid report info.\nIs debuginfo missing?"))

    def report_done(self, result):
        self.emit("report-done", result)

    def start_job(self, crash_id, force=0):
        # 2nd param is "force recreating of backtrace etc"
        self.daemon().StartJob(crash_id, force, timeout=60)
        self.job_crash_id = crash_id

    def Report(self, report, reporters, reporters_settings = None):
        # map < Plugin_name vec <status, message> >
        # daemon expects plugin names, not the objects
        reporters_names = [str(reporter) for reporter in reporters]
        if reporters_settings:
            self.daemon().Report(report, reporters_names, reporters_settings, reply_handler=self.report_done, error_handler=self.error_handler_cb, timeout=60)
        else:
            self.daemon().Report(report, reporters_names, reply_handler=self.report_done, error_handler=self.error_handler_cb, timeout=60)

    def DeleteDebugDump(self, crash_id):
        return self.daemon().DeleteDebugDump(crash_id)

    def getDumps(self):
        row_dict = None
        rows = []
        # FIXME check the arguments
        for row in self.daemon().GetCrashInfos():
            row_dict = {}
            for column in row:
                row_dict[column] = row[column]
            rows.append(row_dict)
        return rows

    def getPluginsInfo(self):
        return self.daemon().GetPluginsInfo()

    def getPluginSettings(self, plugin_name):
        settings = self.daemon().GetPluginSettings(plugin_name)
        return settings

# "Enable" toggling in GUI is disabled for now. Grep for PLUGIN_DYNAMIC_LOAD_UNLOAD
#    def registerPlugin(self, plugin_name):
#        return self.daemon().RegisterPlugin(plugin_name)
#
#    def unRegisterPlugin(self, plugin_name):
#        return self.daemon().UnRegisterPlugin(plugin_name)

    def setPluginSettings(self, plugin_name, plugin_settings):
        return self.daemon().SetPluginSettings(plugin_name, plugin_settings)

    def getSettings(self):
        return self.daemon().GetSettings()

    def setSettings(self, settings):
        # FIXME: STUB!!!!
        log1("setSettings stub")
        retval = self.daemon().SetSettings(self.daemon().GetSettings())
        print ">>>", retval
