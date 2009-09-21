# -*- coding: utf-8 -*-
import pygtk
pygtk.require("2.0")
import gtk #, pango
import gtk.glade
import pango
import sys
from CC_gui_functions import *
from CCReport import Report
import CellRenderers
#from CCDumpList import getDumpList, DumpList
from abrt_utils import _

# FIXME - create method or smth that returns type|editable|content
TYPE = 0
EDITABLE = 1
CONTENT = 2

class ReporterDialog():
    """Reporter window"""
    def __init__(self, report):
        self.editable = []
        self.row_dict = {}
        self.report = report
        #Set the Glade file
        # FIXME add to path
        self.gladefile = "%s%sreport.glade" % (sys.path[0],"/")
        self.wTree = gtk.glade.XML(self.gladefile)
        #Get the Main Window, and connect the "destroy" event
        self.window = self.wTree.get_widget("reporter_dialog")
        self.window.set_default_size(640, 480)

        # comment textview
        self.tvComment = self.wTree.get_widget("tvComment")
        self.tvComment.connect("focus-in-event", self.on_comment_focus_cb)
        self.comment_changed = False

        self.tvReport = self.wTree.get_widget("tvReport")

        self.reportListStore = gtk.ListStore(str, str, bool, bool, bool)
        # set filter
        #self.modelfilter = self.reportListStore.filter_new()
        #self.modelfilter.set_visible_func(self.filter_reports, None)
        self.tvReport.set_model(self.reportListStore)
        renderer = gtk.CellRendererText()
        column = gtk.TreeViewColumn('Item', renderer, text=0)
        self.tvReport.append_column(column)

        renderer = CellRenderers.MultilineCellRenderer()
        renderer.props.editable = True
        renderer.props.wrap_mode = pango.WRAP_WORD
        renderer.props.wrap_width = 800

        #renderer.props.wrap_mode = pango.WRAP_WORD
        #renderer.props.wrap_width = 600
        column = gtk.TreeViewColumn('Value', renderer, text=1, editable=2)
        self.tvReport.append_column(column)
        renderer.connect('edited',self.column_edited,self.reportListStore)
        # toggle
        toggle_renderer = gtk.CellRendererToggle()
        toggle_renderer.set_property('activatable', True)
        toggle_renderer.connect( 'toggled', self.on_send_toggled, self.reportListStore )
        column = gtk.TreeViewColumn('Send', toggle_renderer)
        column.add_attribute( toggle_renderer, "active", 3)
        column.add_attribute( toggle_renderer, "visible", 4)
        self.tvReport.insert_column(column,0)
        # connect the signals
        self.wTree.get_widget("bApply").connect("clicked", self.on_apply_clicked, self.tvReport)
        #self.wTree.get_widget("bCancel").connect("clicked", self.on_cancel_clicked, self.tvReport)

        self.tvReport.connect_after("size-allocate", self.on_window_resize)

        self.hydrate()

    def on_send_toggled(self, cell, path, model):
        model[path][3] = not model[path][3]

    def on_comment_focus_cb(self, widget, event):
        if not self.comment_changed:
            widget.set_buffer(gtk.TextBuffer())
            self.comment_changed = True

    def on_window_resize(self, treeview, allocation):
        # multine support
        pass
        #print allocation

    def column_edited(self, cell, path, new_text, model):
        # 1 means the second cell
        model[path][1] = new_text
        return

    def on_apply_clicked(self, button, treeview):
        attributes = ["item", "content", "editable", "send", "attachment"]
        for row in self.reportListStore:
            rowe = dict(zip(attributes, row))
            if (rowe["attachment"] or (rowe["editable"] and rowe["attachment"])) and rowe["send"]:
                result = gui_question_dialog(_("<b>WARNING</b>, you're about to send data which might contain sensitive information.\n"
                                        "Do you really want to send <b>%s</b>?\n" % rowe["item"]), self.window)
                if result == gtk.RESPONSE_NO:
                    row[attributes.index("send")] = False
        self.dehydrate()

    def on_cancel_clicked(self, button, treeview):
        pass

    def hydrate(self):
        for item in self.report:
            if item == "Comment":
                buff = gtk.TextBuffer()
                comment = _("Brief description how to reproduce this or what you did...")
                try:
                    if self.report[item][CONTENT]:
                        comment = self.report[item][CONTENT]
                        self.comment_changed = True
                except Exception, e:
                    pass
                
                buff.set_text(comment)
                
                self.tvComment.set_buffer(buff)
                continue
            if self.report[item][TYPE] != 's':
                # item name 0| value 1| editable? 2| toggled? 3| visible?(attachment)4
                if self.report[item][EDITABLE] == 'y':
                    self.editable.append(item)
                self.row_dict[item] = self.reportListStore.append([item, self.report[item][CONTENT],
                                                                    item in self.editable, True,
                                                                    self.report[item][TYPE] in ['a','b']])

    def dehydrate(self):
        attributes = ["item", "content", "editable", "send", "attachment"]
        for row in self.reportListStore:
            rowe = dict(zip(attributes, row))
            if not rowe["editable"] and not rowe["attachment"]:
                self.report[rowe["item"]][CONTENT] = rowe["content"]
            elif rowe["editable"] and not rowe["attachment"]:
                self.report[rowe["item"]][CONTENT] = rowe["content"]
            elif (rowe["attachment"] or (rowe["editable"] and rowe["attachment"])) and rowe["send"]:
                self.report[rowe["item"]][CONTENT] = rowe["content"]
            else:
                del self.report[rowe["item"]]
        if self.comment_changed:
            buff = self.tvComment.get_buffer()
            self.report["Comment"] = ['t', 'y', buff.get_text(buff.get_start_iter(),buff.get_end_iter())]
        else:
            del self.report["Comment"]

    def run(self):
        result = self.window.run()
        if result != gtk.RESPONSE_APPLY:
            self.report = None
        self.window.destroy()
        return self.report

