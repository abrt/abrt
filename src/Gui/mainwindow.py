#!/usr/bin/env python

import sys
import pygtk
pygtk.require("2.0")
import gtk
import gtk.glade
import CCGuiDbusBackend
from datetime import datetime

def cb(self, *args):
    pass

class CCMainWindow():
    """This is an Hello World GTK application"""

    def __init__(self):
        self.ccdaemon = CCGuiDbusBackend.DBusManager()
        #Set the Glade file
        self.gladefile = "ccgui.glade"  
        self.wTree = gtk.glade.XML(self.gladefile) 
        
        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("main_window")
    #    self.window.set_default_size(640, 480)
        if (self.window):
            self.window.connect("destroy", gtk.main_quit)
        
        #init the dumps treeview
        self.dlist = self.wTree.get_widget("tvDumps")
        columns = [None]*2
        columns[0] = gtk.TreeViewColumn('Date')
        columns[1] = gtk.TreeViewColumn('package')
        # create list
        self.dumpsListStore = gtk.ListStore(str, str, int)
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
        
    def filter_dumps(self, model, miter, data):
        # this could be use for filtering the dumps
        return True
        
    def show(self):
        self.window.show()
    
    def hydrate(self):
        self.rows = self.ccdaemon.getDumps()
        row_c = 0
        for row in self.rows:
            self.dumpsListStore.append([row["Time"], row["Package"], row_c])
            row_c += 1
    
    def on_tvDumps_cursor_changed(self,treeview):
        dumpsListStore, path = self.dlist.get_selection().get_selected_rows()
        if not path:
            return

        # rewrite this OO
        #DumpList class
        row = self.rows[dumpsListStore.get_value(dumpsListStore.get_iter(path[0]), 2)]
        
        lDate = self.wTree.get_widget("lDate")
        #move this to Dump class
        t = datetime.fromtimestamp(int(row["Time"]))
        date = t.strftime("%Y-%m-%d %H:%M:%S")
        lDate.set_label(date)
        lPackage = self.wTree.get_widget("lPackage")
        lPackage.set_label(row["Package"])
        self.wTree.get_widget("lExecutable").set_label(row["Executable"])
        self.wTree.get_widget("lCRate").set_label(row["Count"])
        #print self.rows[row]
        
    def on_bDelete_clicked(self, button):
        print "Delete"
        
    def on_bNext_clicked(self, button):
        print "Next"
    
    def on_bQuit_clicked(self, button):
        print "Quit"
        gtk.main_quit()
    

if __name__ == "__main__":
    cc = CCMainWindow()
    cc.hydrate()
    cc.show()
    gtk.main()

