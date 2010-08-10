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
from CReporterAssistant import ReporterAssistant
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
            self.window.set_default_size(600, 700)
            self.window.connect("delete_event", self.delete_event_cb)
            self.window.connect("destroy", self.destroy)
            self.window.connect("focus-in-event", self.focus_in_cb)
        self.wTree.get_widget("vp_details").modify_bg(gtk.STATE_NORMAL,gtk.gdk.color_parse("#FFFFFF"))
        #init the dumps treeview
        self.dlist = self.wTree.get_widget("tvDumps")
        #rows of items with:
        STATUS_COL      = 0
        APP_NAME_COL    = 1
        TIME_STR_COL    = 2
        HOSTNAME_COL    = 3
        UNIX_TIME_COL   = 4
        DUMP_OBJECT_COL = 5
        #is_reported, application_name, hostname, date, time_in_sec ?object?
        self.dumpsListStore = gtk.ListStore(str, str, str, str, int, object)
        self.dlist.set_model(self.dumpsListStore)
        # add pixbuff separatelly
        icon_column = gtk.TreeViewColumn(_("Reported"))
        icon_column.cell = gtk.CellRendererPixbuf()
        #icon_column.cell.set_property('cell-background', "#C9C9C9")
        n = self.dlist.append_column(icon_column)
        icon_column.pack_start(icon_column.cell, True)
        icon_column.set_attributes(icon_column.cell, stock_id=(n-1))# cell_background_set=6)
        # ===============================================
        columns = []
        columns.append(gtk.TreeViewColumn(_("Application")))
        columns[-1].set_sort_column_id(APP_NAME_COL)
        columns.append(gtk.TreeViewColumn(_("Hostname")))
        columns[-1].set_sort_column_id(HOSTNAME_COL)
        columns.append(gtk.TreeViewColumn(_("Latest Crash")))
        columns[-1].set_sort_column_id(UNIX_TIME_COL)
        # add cells to colums and bind cells to the liststore values
        for column in columns:
            n = self.dlist.append_column(column)
            column.cell = gtk.CellRendererText()
            column.pack_start(column.cell, False)
            column.set_attributes(column.cell, text=(n-1))
            column.set_resizable(True)
        #connect signals
        self.dlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.dlist.connect("row-activated", self.on_dumpRowActivated)
        self.dlist.connect("button-press-event", self.on_popupActivate)
        self.wTree.get_widget("bDelete").connect("clicked", self.on_bDelete_clicked, self.dlist)
        self.wTree.get_widget("bReport").connect("clicked", self.on_bReport_clicked)
        self.wTree.get_widget("b_close").connect("clicked", self.on_bQuit_clicked)
        self.wTree.get_widget("b_copy").connect("clicked", self.on_b_copy_clicked)
        self.wTree.get_widget("b_help").connect("clicked", self.on_miAbout_clicked)
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
            gui_error_message(_("Cannot show the settings dialog.\n%s" % ex))
            return
        dialog.show()

    def error_cb(self, daemon, message=None):
        gui_error_message(_("Unable to finish the current task!\n%s" % message), parent_dialog=self.window)

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

    def get_username_from_uid(self, uid):
        # if uid == None or "" return it back
        if not uid:
            return uid
        user = "N/A"
        if uid != "-1":   # compat: only abrt <= 1.0.9 used UID = -1
            try:
                user = pwd.getpwuid(int(uid))[0]
            except Exception, ex:
                user = "UID: %s" % uid
        return user


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
            n = self.dumpsListStore.append([["gtk-no","gtk-yes"][entry.isReported()],
                                            entry.getExecutable(),
                                            entry.get_hostname(),
                                            entry.getTime("%c"),
                                            entry.getTime(),
                                            entry])
        # activate the first row if any..
        if n:
            # we can use (0,) as path for the first row, but what if API changes?
            self.dlist.set_cursor(self.dumpsListStore.get_path(self.dumpsListStore.get_iter_first()))

    def filter_dumps(self, model, miter, data):
        # for later..
        return True

    def dumplist_get_selected(self):
        selection = self.dlist.get_selection()
        if selection:
            # returns (dumpsListStore, path) tuple
            dumpsListStore, path = selection.get_selected_rows()
            return dumpsListStore, path
        else:
            return None, None

    def on_tvDumps_cursor_changed(self, treeview):
        dumpsListStore, path = self.dumplist_get_selected()
        if not path:
            self.wTree.get_widget("bDelete").set_sensitive(False)
            self.wTree.get_widget("bReport").set_sensitive(False)
            self.wTree.get_widget("b_copy").set_sensitive(False)
            # create an empty dump to fill the labels with empty strings
            self.wTree.get_widget("sw_details").hide()
            return
        else:
            self.wTree.get_widget("sw_details").show()
            self.wTree.get_widget("bDelete").set_sensitive(True)
            self.wTree.get_widget("bReport").set_sensitive(True)
            self.wTree.get_widget("b_copy").set_sensitive(True)
            # this should work until we keep the row object in the last position
            dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]),
                                            dumpsListStore.get_n_columns()-1)

        try:
            icon = get_icon_for_package(self.theme, dump.getPackageName())
        except:
            icon = None

        i_package_icon = self.wTree.get_widget("i_package_icon")
        if icon:
            i_package_icon.set_from_pixbuf(icon)
        else:
            i_package_icon.set_from_icon_name("application-x-executable", gtk.ICON_SIZE_DIALOG)

        l_heading = self.wTree.get_widget("l_detail_heading")
        l_heading.set_markup(_("<b>%s Crash</b>\n%s") % (dump.getPackageName().title(),dump.getPackage()))

        # process the labels in sw_details
        # hide the fields that are not filled by daemon - e.g. comments
        # and how to reproduce
        for field in dump.not_required_fields:
            self.wTree.get_widget("l_%s" % field.lower()).hide()
            self.wTree.get_widget("l_%s_heading" % field.lower()).hide()

        # fill the details
        # read attributes from CCDump object and if a corresponding label is
        # found, then the label text is set to the attribute's value
        # field names in glade file:
        #   heading label: l_<field>_heading
        #   text label:    l_<field>
        for att in dump.__dict__:
            label = self.wTree.get_widget("l_%s" % str(att).lower())
            if label:
                label.show()
                if att in dump.not_required_fields:
                    try:
                        lbl_heading = self.wTree.get_widget("l_%s_heading" % str(att).lower())
                        lbl_heading.show()
                    except:
                        # we don't care if we fail to show the heading, it will
                        # break the gui a little, but it's better then exit
                        log2("failed to show the heading for >%s< : %s" % (att,e))
                        pass
                if dump.__dict__[att] != None:
                    label.set_text(dump.__dict__[att])
                else:
                    label.set_text("")
        self.wTree.get_widget("l_date").set_text(dump.getTime("%c"))
        self.wTree.get_widget("l_user").set_text(self.get_username_from_uid(dump.getUID()))

        #move this to Dump class
        hb_reports = self.wTree.get_widget("hb_reports")
        lReported = self.wTree.get_widget("l_message")
        if dump.isReported():
            hb_reports.show()
            report_label_raw = ""
            report_label = ""
            # plugin message follows, but at least in case of kerneloops,
            # it is not informative (no URL to the report)
            for message in dump.getMessage().split(';'):
                if message:
                    report_message = tag_urls_in_text(message)
                    report_label += "%s\n" % report_message
                    report_label_raw += "%s\n" % message
            log2("setting markup '%s'", report_label)
            # Sometimes (!) set_markup() fails with
            # "GtkWarning: Failed to set text from markup due to error parsing
            # markup: Unknown tag 'a'" If it does, then set_text()
            # in "fill the details" above acts as a fallback
            lReported.set_markup(report_label)
        else:
            hb_reports.hide()

    def mark_last_selected_row(self, dump_list_store, path, iter, last_selected_uuid):
        # Get dump object from list (in our list it's in last col)
        dump = dump_list_store.get_value(iter, dump_list_store.get_n_columns()-1)
        if dump.getUUID() == last_selected_uuid:
            self.dlist.set_cursor(dump_list_store.get_path(iter)[0])
            return True # done, stop iteration
        return False

    def on_bDelete_clicked(self, button, treeview):
        dumpsListStore, path = self.dumplist_get_selected()
        if not path:
            return
        # this should work until we keep the dump object in the last position
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)
        next_iter = dumpsListStore.iter_next(dumpsListStore.get_iter(path[0]))
        last_dump = None
        if next_iter:
            last_dump = dumpsListStore.get_value(next_iter, dumpsListStore.get_n_columns()-1)
        try:
            self.ccdaemon.DeleteDebugDump("%s:%s" % (dump.getUID(), dump.getUUID()))
            self.hydrate()
            if last_dump:
                # we deleted the selected line, so we want to select the next one
                dumpsListStore.foreach(self.mark_last_selected_row, last_dump.getUUID())
            treeview.emit("cursor-changed")
        except Exception, ex:
            print ex

    def dumplist_get_selected_values(self):
        dumpsListStore, path = self.dumplist_get_selected()
        if path and dumpsListStore:
            return dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)
        return None

    def on_b_copy_clicked(self, button):
        clipboard = gtk.clipboard_get()
        dump = self.dumplist_get_selected_values()
        if not dump:
            gui_info_dialog(_("You have to select a crash to copy."), parent=self.window)
            return
        # dictionaries are not sorted, so we need this as a workaround
        dumpinfo = [("Package:", dump.package),
                    ("Latest Crash:", dump.date),
                    ("Command:", dump.cmdline),
                    ("Reason:", dump.reason),
                    ("Comment:", dump.comment),
                    ("Bug Reports:", dump.Message),
        ]
        dumpinfo_text = ""
        for line in dumpinfo:
            dumpinfo_text += ("%-12s\t%s" % (line[0], line[1])).replace('\n','\n\t\t')
            dumpinfo_text += '\n'
        clipboard.set_text(dumpinfo_text)

    def destroy(self, widget, data=None):
        gtk.main_quit()

    def on_data_changed_cb(self, *_args):
        # FIXME mark the new entry somehow....
        # remember the selected row
        last_dump = None
        dumpsListStore, path = self.dumplist_get_selected()
        if path and dumpsListStore:
            last_dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)
        self.hydrate()
        if last_dump:
            # re-select the line that was selected before a new crash happened
            dumpsListStore.foreach(self.mark_last_selected_row, last_dump.getUUID())


    def on_bReport_clicked(self, button):
        dumpsListStore, path = self.dumplist_get_selected()
        self.on_dumpRowActivated(self.dlist, None, path, None)

    def on_dumpRowActivated(self, treeview, it, path, user_data=None):
        dumpsListStore, path = self.dumplist_get_selected()
        if not path:
            return
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), dumpsListStore.get_n_columns()-1)

        # Do we want to let user decide which UI they want to use?
        #rs = ReporterSelector(dump, self.ccdaemon, parent=self.window)
        #rs.show()
        assistant = ReporterAssistant(dump, self.ccdaemon, parent=self.window)
        assistant.hydrate()

    def delete_event_cb(self, widget, event, data=None):
        gtk.main_quit()

    def focus_in_cb(self, widget, event, data=None):
        self.window.set_urgency_hint(False)

    def on_bQuit_clicked(self, widget):
        try:
            gtk.main_quit()
        except: # prevent "RuntimeError: called outside of a mainloop"
            sys.exit()

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
        opts, args = getopt.getopt(sys.argv[1:], "vh", ["help", "report="])
    except getopt.GetoptError, err:
        print str(err) # prints something like "option -a not recognized"
        sys.exit(2)

    for opt, arg in opts:
        if opt == "-v":
            verbose += 1
        elif opt == "--report":
            crashid=arg
        elif opt in ("-h", "--help"):
            print _("Usage: abrt-gui [OPTIONS]"
            "\n\t-v[vv]\t\t\tVerbose"
            "\n\t--report=CRASH_ID\tDirectly report crash with CRASH_ID"
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
            gui_error_message(_("No such crash in the database, probably wrong crashid."
                                "\ncrashid=%s" % crashid))
            sys.exit()
        assistant = ReporterAssistant(crashdump, daemon, parent=None)
        assistant.hydrate()
        # Do we want to let the users to decide which UI to use?
#        rs = ReporterSelector(crashdump, daemon, parent=None)
#        rs.show()
    else:
        cc = MainWindow(daemon)
        cc.hydrate()
        cc.show()
    gtk.main()
