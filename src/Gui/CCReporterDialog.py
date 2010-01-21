# -*- coding: utf-8 -*-
import pygtk
pygtk.require("2.0")
import gtk #, pango
import gtk.glade
import pango
import sys
from CC_gui_functions import *
import CellRenderers
from ABRTPlugin import PluginInfo
from PluginSettingsUI import PluginSettingsUI
from PluginList import getPluginInfoList
#from CCDumpList import getDumpList, DumpList
from CCDump import *   # FILENAME_xxx, CD_xxx
from abrt_utils import _, log, log1, log2

# FIXME - create method or smth that returns type|editable|content

# response
REFRESH = -50
SHOW_LOG = -60

class ReporterDialog():
    """Reporter window"""
    def __init__(self, report, daemon, log=None, parent=None):
        self.editable = []
        self.row_dict = {}
        self.report = report
        #Set the Glade file
        # FIXME add to path
        builderfile = "%s/report.glade" % sys.path[0]
        self.builder = gtk.Builder()
        self.builder.add_from_file(builderfile)
        #Get the Main Window, and connect the "destroy" event
        self.window = self.builder.get_object("reporter_dialog")
        self.window.connect("response", self.on_response, daemon)
        if parent:
            self.window.set_transient_for(parent)
            self.window.set_modal(True)

        # comment textview
        self.tvComment = self.builder.get_object("tvComment")
        self.tvComment.connect("focus-in-event", self.on_comment_focus_cb)
        self.show_hint_comment = 1

        # "how to reproduce" textview
        self.tevHowToReproduce = self.builder.get_object("tevHowToReproduce")

        self.builder.get_object("fErrors").hide()
        self.builder.get_object("bLog").connect("clicked", self.show_log_cb, log)
        self.builder.get_object("cbSendBacktrace").connect("toggled", self.on_send_backtrace_toggled)
        self.allow_send()
        self.hydrate()

    def check_backtrace(self):
        print "checking backtrace"

    def warn_user(self, warnings):
        # FIXME: show in lError
        fErrors = self.builder.get_object("fErrors")
        lErrors = self.builder.get_object("lErrors")
        warning_lbl = None
        for warning in warnings:
            if warning_lbl:
                warning_lbl += "\n* %s" % warning
            else:
                warning_lbl = "* %s" % warning
        lErrors.set_label(warning_lbl)
        fErrors.show_all()

    def hide_warning(self):
        fErrors = self.builder.get_object("fErrors")
        lErrors = self.builder.get_object("lErrors")
        fErrors.hide()

    def allow_send(self):
        self.hide_warning()
        bSend = self.builder.get_object("bSend")
        SendBacktrace = self.builder.get_object("cbSendBacktrace").get_active()
        send = True
        error_msgs = []
        try:
            rating = self.report[FILENAME_RATING]
        except:
            rating = None
        # active buttons acording to required fields
        # if an backtrace has rating use it
        if not SendBacktrace:
            send = False
            error_msgs.append(_("You must agree with submitting the backtrace."))
        # we have both SendBacktrace and rating
        if rating:
            try:
                package = self.report[FILENAME_PACKAGE][CD_CONTENT]
            # if we don't have package for some reason
            except:
                package = None
            # not usable report
            if int(self.report[FILENAME_RATING][CD_CONTENT]) < 3:
                if package:
                    error_msgs.append(_("Reporting disabled because the backtrace is unusable.\nPlease try to install debuginfo manually using command: <b>debuginfo-install %s</b> \nthen use Refresh button to regenerate the backtrace." % package[0:package.rfind('-',0,package.rfind('-'))]))
                else:
                    error_msgs.append(_("The backtrace is unusable, you can't report this!"))
            # probably usable 3
            elif int(self.report[FILENAME_RATING][CD_CONTENT]) < 4:
                error_msgs.append(_("The backtrace is incomplete, please make sure you provide good steps to reproduce."))

        if error_msgs:
            self.warn_user(error_msgs)
        bSend.set_sensitive(send)

    def on_send_backtrace_toggled(self, toggle_button):
        self.allow_send()

    def show_log_cb(self, widget, log):
        show_log(log, parent=self.window)

    # this callback is called when user press Cancel or Report button in Report dialog
    def on_response(self, dialog, response_id, daemon):
        # the button has been pressed (probably)
        if response_id == gtk.RESPONSE_APPLY:
            if not (self.check_settings(daemon) and self.check_report()):
                dialog.stop_emission("response")
                self.builder.get_object("bSend").stop_emission("clicked")
        if response_id == SHOW_LOG:
        # prevent dialog from quitting the run()
            dialog.stop_emission("response")

    def on_send_toggled(self, cell, path, model):
        model[path][3] = not model[path][3]

    def on_comment_focus_cb(self, widget, event):
        if self.show_hint_comment:
            # clear "hint" text by supplying a fresh, empty TextBuffer
            widget.set_buffer(gtk.TextBuffer())
            self.show_hint_comment = 0

    def on_config_plugin_clicked(self, button, plugin, image):
        ui = PluginSettingsUI(plugin, parent=self.window)
        ui.hydrate()
        response = ui.run()
        if response == gtk.RESPONSE_APPLY:
            ui.dehydrate()
            if plugin.Settings.check():
                try:
                    plugin.save_settings_on_client_side()
                except Exception, e:
                    gui_error_message(_("Can't save plugin settings:\n %s" % e))
                box = image.get_parent()
                im = gtk.Image()
                im.set_from_stock(gtk.STOCK_APPLY, gtk.ICON_SIZE_MENU)
                box.remove(image)
                box.pack_start(im, expand = False, fill = False)
                im.show()
                image.destroy()
                button.set_sensitive(False)
        elif response == gtk.RESPONSE_CANCEL:
            print "cancel"
        ui.destroy()

    def check_settings(self, daemon):
        pluginlist = getPluginInfoList(daemon)
        reporters = pluginlist.getReporterPlugins()
        wrong_conf_plugs = []
        for reporter in reporters:
            if reporter.Settings.check() == False:
                wrong_conf_plugs.append(reporter)

        if wrong_conf_plugs:
            gladefile = "%s%ssettings_wizard.glade" % (sys.path[0],"/")
            builder = gtk.Builder()
            builder.add_from_file(gladefile)
            dialog = builder.get_object("WrongSettings")
            vbWrongSettings = builder.get_object("vbWrongSettings")
            for plugin in wrong_conf_plugs:
                hbox = gtk.HBox()
                hbox.set_spacing(6)
                image = gtk.Image()
                image.set_from_stock(gtk.STOCK_CANCEL, gtk.ICON_SIZE_MENU)
                button = gtk.Button(plugin.getName())
                button.connect("clicked", self.on_config_plugin_clicked, plugin, image)
                hbox.pack_start(button)
                hbox.pack_start(image, expand = False, fill = False)
                vbWrongSettings.pack_start(hbox)
            vbWrongSettings.show_all()
            dialog.set_transient_for(self.window)
            dialog.set_modal(True)
            response = dialog.run()
            dialog.destroy()
            if response == gtk.RESPONSE_NO:
                # user cancelled reporting
                return False
            if response == gtk.RESPONSE_YES:
                # "user wants to proceed with report"
                return True
        return True

    def set_label(self, label_widget, text):
        if len(text) > label_widget.get_max_width_chars():
            label_widget.set_tooltip_text(text)
        label_widget.set_text(text)

    def hydrate(self):
        self.editable = []
        self.old_comment = ""
        self.old_how_to_reproduce = ""
        for item in self.report:
            try:
                log2("report[%s]:%s/%s/%s", item, self.report[item][0], self.report[item][1], self.report[item][2][0:20])
            except:
                pass

            if item == FILENAME_BACKTRACE:
                buff = gtk.TextBuffer()
                tvBacktrace = self.builder.get_object("tvBacktrace")
                buff.set_text(self.report[item][CD_CONTENT])
                tvBacktrace.set_buffer(buff)
                continue

            if item == FILENAME_COMMENT:
                try:
                    if self.report[item][CD_CONTENT]:
                        self.old_comment = self.report[item][CD_CONTENT]
                except Exception, e:
                    pass
                continue

            if item == FILENAME_REPRODUCE:
                try:
                    if self.report[item][CD_CONTENT]:
                        self.old_how_to_reproduce = self.report[item][CD_CONTENT]
                except Exception, e:
                    pass
                continue

            if self.report[item][CD_TYPE] == CD_SYS:
                continue

            # item name 0| value 1| editable? 2| toggled? 3| visible?(attachment)4
            # FIXME: handle editable fields
            if self.report[item][CD_TYPE] == CD_BIN:
                self.builder.get_object("fAttachment").show()
                vbAttachments = self.builder.get_object("vbAttachments")
                toggle = gtk.CheckButton(self.report[item][CD_CONTENT])
                vbAttachments.pack_start(toggle)
                # bind item to checkbox
                toggle.item = item
                toggle.show()
                continue

            # It must be CD_TXT field
            item_label = self.builder.get_object("l%s" % item)
            if item_label:
                self.set_label(item_label, self.report[item][CD_CONTENT])
            else:
                # no widget to show this item
                # probably some new item need to adjust the GUI!
                # FIXME: add some window+button to show all the info
                # in raw form (smth like the old report dialog)
                pass
        #end for

        buff = gtk.TextBuffer()
        self.show_hint_comment = (self.old_comment == "")
        if self.show_hint_comment:
            buff.set_text(_("Brief description how to reproduce this or what you did..."))
        else:
            buff.set_text(self.old_comment)
        self.tvComment.set_buffer(buff)

        buff = gtk.TextBuffer()
        if self.old_how_to_reproduce == "":
            buff.set_text("1.\n2.\n3.\n")
        else:
            buff.set_text(self.old_how_to_reproduce)
        self.tevHowToReproduce.set_buffer(buff)

    def dehydrate(self):
        # handle attachments
        vbAttachments = self.builder.get_object("vbAttachments")
        for attachment in vbAttachments.get_children():
            #print "%s file %s" % (["not sending","sending"][attachment.get_active()], attachment.get_label())
            del self.report[attachment.item]

        # handle comment
        buff = self.tvComment.get_buffer()
        text = buff.get_text(buff.get_start_iter(), buff.get_end_iter())
        if self.old_comment != text:
            self.report[FILENAME_COMMENT] = [CD_TXT, 'y', text]
        # handle how to reproduce
        buff = self.tevHowToReproduce.get_buffer()
        text = buff.get_text(buff.get_start_iter(), buff.get_end_iter())
        if self.old_how_to_reproduce != text:
            self.report[FILENAME_REPRODUCE] = [CD_TXT, 'y', text]
        #handle backtrace
        tev_backtrace = self.builder.get_object("tvBacktrace")
        buff = tev_backtrace.get_buffer()
        text = buff.get_text(buff.get_start_iter(), buff.get_end_iter())
        self.report[FILENAME_BACKTRACE] = [CD_TXT, 'y', text]

    def check_report(self):
    # FIXME: check the report for passwords and some other potentially
    # sensitive info
        self.dehydrate()
        return True

    def run(self):
        result = self.window.run()
        self.window.destroy()
        return (result, self.report)
