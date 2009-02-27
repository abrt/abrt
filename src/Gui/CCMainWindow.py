import sys
import pygtk
pygtk.require("2.0")
import gtk
import gtk.glade
import CCDBusBackend
import sys
from CC_gui_functions import *
from CCDumpList import getDumpList, DumpList
from CCReporterDialog import ReporterDialog
from CCReport import Report

def cb(self, *args):
    pass
class MainWindow():
    def __init__(self):
        try:
            self.ccdaemon = CCDBusBackend.DBusManager()
        except Exception, e:
            # show error message if connection fails
            # FIXME add an option to start the daemon
            gui_error_message(e.message)
            sys.exit()
        #Set the Glade file
        # FIXME add to PATH
        self.gladefile = "/usr/share/crash-catcher/ccgui.glade"  
        self.wTree = gtk.glade.XML(self.gladefile) 
        
        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("main_window")
    #    self.window.set_default_size(640, 480)
        if (self.window):
            self.window.connect("destroy", gtk.main_quit)
        
        self.appBar = self.wTree.get_widget("appBar")
        
        #init the dumps treeview
        self.dlist = self.wTree.get_widget("tvDumps")
        columns = [None]*2
        columns[0] = gtk.TreeViewColumn('Date')
        columns[1] = gtk.TreeViewColumn('Package')
        # create list
        self.dumpsListStore = gtk.ListStore(str, str, object)
        # set filter
        self.modelfilter = self.dumpsListStore.filter_new()
        self.modelfilter.set_visible_func(self.filter_dumps, None)
        self.dlist.set_model(self.modelfilter)
        for column in columns:
            n = self.dlist.append_column(column)
            column.cell = gtk.CellRendererText()
            column.pack_start(column.cell, False)
            column.set_attributes(column.cell, text=(n-1))
            column.set_resizable(True)
        
        #connect signals
        self.dlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.wTree.get_widget("bDelete").connect("clicked", self.on_bDelete_clicked)
        self.wTree.get_widget("bNext").connect("clicked", self.on_bNext_clicked)
        self.wTree.get_widget("bQuit").connect("clicked", self.on_bQuit_clicked)
        self.ccdaemon.connect("crash", self.on_data_changed_cb, None)
        
        # load data
        self.load()
    
    def load(self):
        self.appBar.push(0,"Loading dumps...")
        self.loadDumpList()
        self.appBar.pop(0)
        
    def loadDumpList(self):
        #dumplist = getDumpList(dbmanager=self.ccdaemon)
        pass
    
    def on_data_changed_cb(self, *args):
        ret = gui_info_dialog("Another crash detected, do you want to refresh the data?",self.window)
        if ret == gtk.RESPONSE_YES:
            self.hydrate()
        else:
            pass
        #print "got another crash, refresh gui?"
    
    
    def filter_dumps(self, model, miter, data):
        # this could be use for filtering the dumps
        return True
        
    def show(self):
        self.window.show()
    
    def hydrate(self):
        self.dumpsListStore.clear()
        dumplist = getDumpList(self.ccdaemon, refresh=True)
        #self.rows = self.ccdaemon.getDumps()
        #row_c = 0
        for entry in dumplist:
            self.dumpsListStore.append([entry.getTime("%m.%d."),entry.getPackage(),entry])
            #row_c += 1
    
    def on_tvDumps_cursor_changed(self,treeview):
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        if not path:
            return

        # this should work until we keep the row object in the last position
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), len(self.dlist.get_columns()))
        
        lDate = self.wTree.get_widget("lDate")
        #move this to Dump class
        lDate.set_label(dump.getTime("%Y.%m.%d %H:%M:%S"))
        lPackage = self.wTree.get_widget("lPackage")
        lPackage.set_label(dump.getPackage())
        self.wTree.get_widget("lExecutable").set_label(dump.getExecutable())
        self.wTree.get_widget("lCRate").set_label(dump.getCount())
        #print self.rows[row]
        
    def on_bDelete_clicked(self, button):
        print "Delete"
        
    def on_bNext_clicked(self, button):
        # FIXME don't duplicate the code, move to function
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        if not path:
            return
        dump = dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), len(self.dlist.get_columns()))
        # show the report window with selected dump
        try:
            report = self.ccdaemon.getReport(dump.getUUID())
        except Exception,e:
            # FIXME #3	dbus.exceptions.DBusException: org.freedesktop.DBus.Error.NoReply: Did not receive a reply
            # do this async and wait for yum to end with debuginfoinstal
            gui_error_message("Operation taking too long - \nPlease try again after debuginfo is installed")
            return
            
        if not report:
            gui_error_message("Unable to get report! Debuginfo missing?")
            return
        report_dialog = ReporterDialog(report)
        result = report_dialog.run()
        if result == gtk.RESPONSE_CANCEL:
            pass
        else:
            self.ccdaemon.Report(result)
    
    def on_bQuit_clicked(self, button):
        gtk.main_quit()
        
if __name__ == "__main__":
    cc = MainWindow()
    cc.hydrate()
    cc.show()
    gtk.main()

