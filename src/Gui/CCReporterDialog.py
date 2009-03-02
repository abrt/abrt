import pygtk
pygtk.require("2.0")
import gtk #, pango
import gtk.glade
import sys
from CC_gui_functions import *
from CCReport import Report
#from CCDumpList import getDumpList, DumpList

class ReporterDialog():
    """Reporter window"""
    def __init__(self, report):
        self.report = report
        #Set the Glade file
        # FIXME add to path
        self.gladefile = "/usr/share/crash-catcher/report.glade"  
        self.wTree = gtk.glade.XML(self.gladefile) 
        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("reporter_dialog")
        self.window.set_default_size(640, 480)
        
        #init the reports treeview
        self.tvReport = self.wTree.get_widget("tvReport")
        columns = [None]*2
        columns[0] = gtk.TreeViewColumn('Item')
        columns[1] = gtk.TreeViewColumn('Value')
        
        self.reportListStore = gtk.ListStore(str, str, bool)
        # set filter
        #self.modelfilter = self.reportListStore.filter_new()
        #self.modelfilter.set_visible_func(self.filter_reports, None)
        self.tvReport.set_model(self.reportListStore)
        renderer = gtk.CellRendererText()
        column = gtk.TreeViewColumn('Item', renderer, text=0)
        self.tvReport.append_column(column)
        
        renderer = gtk.CellRendererText()
        #renderer.props.wrap_mode = pango.WRAP_WORD
        #renderer.props.wrap_width = 600
        column = gtk.TreeViewColumn('Value', renderer, text=1, editable=2)
        self.tvReport.append_column(column)
        renderer.connect('edited',self.column_edited,self.reportListStore)
        
        # connect the signals
        #self.wTree.get_widget("bApply").connect("clicked", self.on_apply_clicked, self.tvReport)
        #self.wTree.get_widget("bCancel").connect("clicked", self.on_cancel_clicked, self.tvReport)
        
        self.tvReport.connect_after("size-allocate", self.on_window_resize)
        
        self.hydrate()
    
    def on_window_resize(self, treeview, allocation):
        # multine support
        pass
        #print allocation
    
    def column_edited(self, cell, path, new_text, model):
        # 1 means the second cell
        model[path][1] = new_text
        return
        
    def on_apply_clicked(self, button, treeview):
        pass
        
    def on_cancel_clicked(self, button, treeview):
        pass
    
    def hydrate(self):
        editable = ["Comment", "TextData1", "TextData2"]
        for item in self.report:
            self.reportListStore.append([item, self.report[item], item in editable])
        #self.reportListStore.append(["Comment","", True])
    
    def run(self):
        result = self.window.run()
        if result == gtk.RESPONSE_CANCEL:
            self.window.destroy()
            return result
        else:
            self.window.destroy()
            return self.report
    
