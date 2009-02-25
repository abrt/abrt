import pygtk
pygtk.require("2.0")
import gtk
import gtk.glade
import sys
from CC_gui_functions import *
from CCDumpList import getDumpList, DumpList

class ReporterDialog():
    """Reporter window"""
    def __init__(self, dump):
        self.dump = dump
        #Set the Glade file
        # FIXME add to path
        self.gladefile = "../share/crash-catcher/ccgui.glade"  
        self.wTree = gtk.glade.XML(self.gladefile) 
        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("reporter_dialog")
        
        #init the dumps treeview
        self.tvReport = self.wTree.get_widget("tvReport")
        columns = [None]*2
        columns[0] = gtk.TreeViewColumn('Item')
        columns[1] = gtk.TreeViewColumn('Value')
        
        self.reportListStore = gtk.ListStore(str, str, bool)
        # set filter
        #self.modelfilter = self.reportListStore.filter_new()
        #self.modelfilter.set_visible_func(self.filter_dumps, None)
        self.tvReport.set_model(self.reportListStore)
        renderer = gtk.CellRendererText()
        column = gtk.TreeViewColumn('Item', renderer, text=0)
        self.tvReport.append_column(column)
        
        renderer = gtk.CellRendererText()
        column = gtk.TreeViewColumn('Value', renderer, text=1, editable=2)
        self.tvReport.append_column(column)
        renderer.connect('edited',self.column_edited,self.reportListStore)
        
        # connect the signals
        self.wTree.get_widget("bApply").connect("clicked", self.on_apply_clicked, self.tvReport)
        
        self.hydrate()

    def column_edited(self, cell, path, new_text, model):
        # 1 means the second cell
        model[path][1] = new_text
        return
        
    def on_apply_clicked(self, button, treeview):
        #print treeview
        self.window.hide()
        
    def hydrate(self):
        for item in self.dump.__dict__:
            self.reportListStore.append([item, self.dump.__dict__[item], False])
        self.reportListStore.append(["Comment","", True])
    
    def run(self):
        self.window.show()
    
    
