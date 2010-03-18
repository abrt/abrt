# -*- coding: utf-8 -*-
import sys
import pwd
import getopt

from abrt_utils import _, init_logging, log, log1, log2
import gobject
gobject.set_prgname(_("Automatic Bug Reporting Tool"))
import pygtk
pygtk.require("2.0")
try:
    import gtk
except RuntimeError,e:
    # rhbz#552039
    print e
    sys.exit()
import gtk.glade

from ConfBackend import getCurrentConfBackend, ConfBackendInitError
import CCDBusBackend
from CC_gui_functions import *
from CCDumpList import getDumpList
from CCDump import *   # FILENAME_xxx, CD_xxx
from CCReporterDialog import ReporterDialog, ReporterSelector
from PluginsSettingsDialog import PluginsSettingsDialog
from SettingsDialog import SettingsDialog
from PluginList import getPluginInfoList
import ABRTExceptions


class MainWindow():
    ccdaemon = None
    def __init__(self, daemon):
        self.theme = gtk.icon_theme_get_default()
        self.updates = ""
        self.ccdaemon = daemon
        #Set the Glade file
        self.gladefile = "%s/ccgui.glade" % sys.path[0]
        self.wTree = gtk.glade.XML(self.gladefile)

        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("main_window")
        if self.window:
            self.window.set_default_size(700, 480)
            self.window.connect("delete_event", self.delete_event_cb)
            self.window.connect("destroy", self.destroy)
            self.window.connect("focus-in-event", self.focus_in_cb)

        #init the dumps treeview
        self.dlist = self.wTree.get_widget("tvDumps")
        #rows of items with:
        ICON_COL = 0
        PACKAGE_COL = 1
        APPLICATION_COL = 2
        TIME_STR_COL = 3
        CRASH_RATE_COL = 4
        USER_COL = 5
        IS_REPORTED_COL = 6
        UNIX_TIME_COL = 7
        DUMP_OBJECT_COL = 8
        #icon, package_name, application, date, crash_rate, user, is_reported, time_in_sec ?object?
        self.dumpsListStore = gtk.ListStore(gtk.gdk.Pixbuf, str,str,str,int,str,bool, int, object)
        self.dlist.set_model(self.dumpsListStore)
        # add pixbuff separatelly
        icon_column = gtk.TreeViewColumn(_("Icon"))
        icon_column.cell = gtk.CellRendererPixbuf()
        icon_column.cell.set_property('cell-background', "#C9C9C9")
        n = self.dlist.append_column(icon_column)
        icon_column.pack_start(icon_column.cell, False)
        icon_column.set_attributes(icon_column.cell, pixbuf=(n-1), cell_background_set=6)
        # ===============================================
        columns = []
        columns.append(gtk.TreeViewColumn(_("Package")))
        columns[-1].set_sort_column_id(PACKAGE_COL)
        columns.append(gtk.TreeViewColumn(_("Application")))
        columns[-1].set_sort_column_id(APPLICATION_COL)
        columns.append(gtk.TreeViewColumn(_("Date")))
        columns[-1].set_sort_column_id(UNIX_TIME_COL)
        columns.append(gtk.TreeViewColumn(_("Crash count")))
        columns[-1].set_sort_column_id(CRASH_RATE_COL)
        columns.append(gtk.TreeViewColumn(_("User")))
        columns[-1].set_sort_column_id(USER_COL)
        # create list
        for column in columns:
            n = self.dlist.append_column(column)
            column.cell = gtk.CellRendererText()
            column.pack_start(column.cell, False)
            #column.set_attributes(column.cell, )
            # FIXME: use some relative indexing
            column.cell.set_property('cell-background', "#C9C9C9")
            column.set_attributes(column.cell, text=(n-1), cell_background_set=6)
            column.set_resizable(True)
        #connect signals
        self.dlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.dlist.connect("row-activated", self.on_dumpRowActivated)
        self.dlist.connect("button-press-event", self.on_popupActivate)
        self.wTree.get_widget("bDelete").connect("clicked", self.on_bDelete_clicked, self.dlist)
        self.wTree.get_widget("bReport").connect("clicked", self.on_bReport_clicked)
        self.wTree.get_widget("miQuit").connect("activate", self.on_bQuit_clicked)
        self.wTree.get_widget("miAbout").connect("activate", self.on_miAbout_clicked)
        self.wTree.get_widget("miPlugins").connect("activate", self.on_miPreferences_clicked)
        self.wTree.get_widget("miPreferences").connect("activate", self.on_miSettings_clicked)
        self.wTree.get_widget("miReport").connect("activate", self.on_bReport_clicked)
        self.wTree.get_widget("miDelete").connect("activate", self.on_bDelete_clicked, self.dlist)
        # connect handlers for daemon signals
        self.ccdaemon.connect("crash", self.on_data_changed_cb, None)
        self.ccdaemon.connect("abrt-error", self.error_cb)
        #self.ccdaemon.connect("update", self.update_cb)
        # for now, just treat them the same (w/o this, we don't even see daemon warnings in logs!):
        #self.ccdaemon.connect("warning", self.update_cb)
        self.ccdaemon.connect("show", self.show_cb)
        self.ccdaemon.connect("daemon-state-changed", self.on_daemon_state_changed_cb)
        self.ccdaemon.connect("report-done", self.on_report_done_cb)

        self.pluginlist = None

    def on_report_done_cb(self, daemon, result):
        self.hydrate()

    def on_daemon_state_changed_cb(self, widget, state):
        if state == "up":
            self.hydrate() # refresh crash list
            #self.window.set_sensitive(True)
        # abrtd might just die on timeout, it's not fatal
        #elif state == "down":
        #    self.window.set_sensitive(False)

    def on_popupActivate(self, widget, event):
        menu = self.wTree.get_widget("popup_menu")
        # 3 == right mouse button
        if event.button == 3:
            menu.popup(None, None, None, event.button, event.time)

    def on_miAbout_clicked(self, widget):
        dialog = self.wTree.get_widget("about")
        result = dialog.run()
        dialog.hide()

    def on_miPreferences_clicked(self, widget):
        dialog = PluginsSettingsDialog(self.window,self.ccdaemon)
        dialog.hydrate()
        dialog.show()

    def on_miSettings_clicked(self, widget):
        dialog = SettingsDialog(self.window, self.ccdaemon)
        try:
            dialog.hydrate()
        except Exception, ex:
            gui_error_message(_("Can't show the settings dialog\n%s" % ex))
            return
        dialog.show()

    def error_cb(self, daemon, message=None):
        gui_error_message(_("Unable to finish current task!\n%s" % message), parent_dialog=self.window)

    def update_cb(self, daemon, message):
        self.updates += message
        if self.updates[-1] != '\n':
            self.updates += '\n'
        message = message.replace('\n',' ')
        self.wTree.get_widget("lStatus").set_text(message)
        buff = gtk.TextBuffer()
        buff.set_text(self.updates)
        end = buff.get_insert()
        tvUpdates = self.wTree.get_widget("tvUpdates")
        tvUpdates.set_buffer(buff)
        tvUpdates.scroll_mark_onscreen(end)

    def hydrate(self):
        n = None
        self.dumpsListStore.clear()
        try:
            dumplist = getDumpList(self.ccdaemon, refresh=True)
        except Exception, ex:
            # there is something wrong with the daemon if we cant get the dumplist
            gui_error_message(_("Error while loading the dumplist.\n%s" % ex))
            # so we shouldn't continue..
            sys.exit()
        for entry in dumplist[::-1]:
            try:
                icon = get_icon_for_package(self.theme, entry.getPackageName())
            except:
                icon = None
            user = "N/A"
            if entry.getUID() != "-1":   # compat: only abrt <= 1.0.9 used UID = -1
                try:
                    user = pwd.getpwuid(int(entry.getUID()))[0]
                except Exception, ex:
                    user = "UID: %s" % entry.getUID()
            n = self.dumpsListStore.append([icon, entry.getPackage(), entry.getExecutable(),
                                            entry.getTime("%c"), entry.getCount(), user, entry.isReported(), entry.getTime(""), entry])
        # activate the first row if any..
        if n:
            # we can use (0,) as path for the first row, but what if API changes?
            self.dlist.set_cursor(self.dumpsListStore.get_path(self.dumpsListStore.get_iter_first()))

    def filter_dumps(self, model, miter, data):
        # for later..
        return True

    def on_tvDumps_cursor_changed(self,treeview):
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        if not path:
            self.wTree.get_widget("bDelete").set_sensitive(False)
            self.wTree.get_widget("bReport").set_sensitive(False)
            return
        self.wTree.get_widget("bDelete").set_sensitive(True)
        self.wTree.get_widget("bReport").set_sensitive(True)
        # this should work until we keep the row object in the last position
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)
        #move this to Dump class
        lReported = self.wTree.get_widget("lReported")
        if dump.isReported():
            report_label_raw = _("This crash has been reported:\n")
            report_label = _("<b>This crash has been reported:</b>\n")
            # plugin message follows, but at least in case of kerneloops,
            # it is not informative (no URL to the report)
            for message in dump.getMessage().split(';'):
                if message:
                    report_message = tag_urls_in_text(message)
                    report_label += "%s\n" % report_message
                    report_label_raw += "%s\n" % message
            log2("setting markup '%s'", report_label)
            lReported.set_text(report_label_raw)
            # Sometimes (!) set_markup() fails with
            # "GtkWarning: Failed to set text from markup due to error parsing markup: Unknown tag 'a'"
            # If it does, then set_text() above acts as a fallback
            lReported.set_markup(report_label)
        else:
            lReported.set_markup(_("<b>Not reported!</b>"))

    def on_bDelete_clicked(self, button, treeview):
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        if not path:
            return
        # this should work until we keep the row object in the last position
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)
        try:
            self.ccdaemon.DeleteDebugDump("%s:%s" % (dump.getUID(), dump.getUUID()))
            self.hydrate()
            treeview.emit("cursor-changed")
        except Exception, ex:
            print ex

    def destroy(self, widget, data=None):
        gtk.main_quit()

    def on_data_changed_cb(self, *_args):
        # FIXME mark the new entry somehow....
        # remember the selected row
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        self.hydrate()
        if not path:
            return
        self.dlist.set_cursor(path[0])

    def on_bReport_clicked(self, button):
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        self.on_dumpRowActivated(self.dlist, None, path, None)

    def on_dumpRowActivated(self, treeview, it, path, user_data=None):
        dumpsListStore, path = treeview.get_selection().get_selected_rows()
        if not path:
            return
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)

        rs = ReporterSelector(dump, self.ccdaemon, parent=self.window)
        rs.show()

    def delete_event_cb(self, widget, event, data=None):
        gtk.main_quit()

    def focus_in_cb(self, widget, event, data=None):
        self.window.set_urgency_hint(False)

    def on_bQuit_clicked(self, widget):
        gtk.main_quit()

    def show(self):
        self.window.show()

    def show_cb(self, daemon):
        if self.window:
            if self.window.is_active():
                return
            self.window.set_urgency_hint(True)
            self.window.present()

if __name__ == "__main__":
    verbose = 0
    crashid = None
    try:
        opts, args = getopt.getopt(sys.argv[1:], "vh", ["help","report="])
    except getopt.GetoptError, err:
        print str(err) # prints something like "option -a not recognized"
        sys.exit(2)

    for opt, arg in opts:
        if opt == "-v":
            verbose += 1
        elif opt == "--report":
            crashid=arg
        elif opt in ("-h","--help"):
            print _("Usage: abrt-gui [OPTIONS]"
            "\n\t-h, --help         \tthis help message"
            "\n\t-v[vv]             \tverbosity level"
            "\n\t--report=<crashid>\tdirectly report crash with crashid=<crashid>"
            )
            sys.exit()

    init_logging("abrt-gui", verbose)
    log1("log level:%d", verbose)

    try:
        daemon = CCDBusBackend.DBusManager()
    except ABRTExceptions.IsRunning:
        # another instance is running, so exit quietly
        sys.exit()
    except Exception, ex:
        # show error message if connection fails
        gui_error_message("%s" % ex)
        sys.exit()

    if crashid:
        dumplist = getDumpList(daemon)
        crashdump = dumplist.getDumpByCrashID(crashid)
        if not crashdump:
            gui_error_message(_("No such crash in database, probably wrong crashid."
                                "\ncrashid=%s" % crashid))
            sys.exit()
        rs = ReporterSelector(crashdump, daemon, parent=None)
        rs.show()
    else:
        cc = MainWindow(daemon)
        cc.hydrate()
        cc.show()
    gtk.main()
