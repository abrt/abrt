# -*- coding: utf-8 -*-
import pygtk
pygtk.require("2.0")
import gtk
import gobject
import sys
from CC_gui_functions import *
import CellRenderers
from ABRTPlugin import PluginInfo
from PluginSettingsUI import PluginSettingsUI
from PluginList import getPluginInfoList
from CCDump import *   # FILENAME_xxx, CD_xxx
from abrt_utils import _, log, log1, log2, get_verbose_level, g_verbose

# FIXME - create method or smth that returns type|editable|content

# response
REFRESH = -50
SHOW_LOG = -60

# default texts
COMMENT_HINT_TEXT = _("Brief description of how to reproduce this or what you did...")
HOW_TO_HINT_TEXT = "1.\n2.\n3.\n"

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
        self.window.set_default_size(-1, 800)
        self.window.connect("response", self.on_response, daemon)
        if parent:
            self.window.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.window.set_transient_for(parent)
            self.window.set_modal(True)
        else:
            self.window.set_position(gtk.WIN_POS_CENTER)

        # comment textview
        self.tvComment = self.builder.get_object("tvComment")
        self.tvComment.connect("focus-in-event", self.on_comment_focus_cb)
        self.show_hint_comment = 1

        # "how to reproduce" textview
        self.tevHowToReproduce = self.builder.get_object("tevHowToReproduce")

        self.builder.get_object("fErrors").hide()
        bLog = self.builder.get_object("bLog")
        #if g_verbose > 0: - doesn't work! why?!
        if get_verbose_level() > 0:
            bLog.connect("clicked", self.show_log_cb, log)
        else:
            bLog.unset_flags(gtk.VISIBLE)
        tb_send_bt = self.builder.get_object("cbSendBacktrace")
        tb_send_bt.connect("toggled", self.on_send_backtrace_toggled)
        try:
            tb_send_bt.get_child().modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse("red"))
        except Exception, ex:
            # we don't want gui to die if it fails to set the button color
            log(ex)
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
            rating = int(self.report[FILENAME_RATING][CD_CONTENT])
        except:
            rating = None
        # active buttons acording to required fields
        # if an backtrace has rating use it
        if not SendBacktrace:
            send = False
            error_msgs.append(_("You must check the backtrace for sensitive data."))
        # we have both SendBacktrace and rating
        if rating != None:
            try:
                package = self.report[FILENAME_PACKAGE][CD_CONTENT]
            # if we don't have package for some reason
            except:
                package = None
            # not usable report
            if int(self.report[FILENAME_RATING][CD_CONTENT]) < 3:
                if package:
                    error_msgs.append(_("Reporting disabled because the backtrace is unusable.\nPlease try to install debuginfo manually using the command: <b>debuginfo-install %s</b> \nthen use the Refresh button to regenerate the backtrace." % package[0:package.rfind('-',0,package.rfind('-'))]))
                else:
                    error_msgs.append(_("The backtrace is unusable, you cannot report this!"))
                send = False
            # probably usable 3
            elif int(self.report[FILENAME_RATING][CD_CONTENT]) < 4:
                error_msgs.append(_("The backtrace is incomplete, please make sure you provide the steps to reproduce."))

        if error_msgs:
            self.warn_user(error_msgs)
        bSend.set_sensitive(send)
        if not send:
            bSend.set_tooltip_text(_("Reporting disabled, please fix the problems shown above."))
        else:
            bSend.set_tooltip_text(_("Sends the report using the selected plugin."))

    def on_send_backtrace_toggled(self, toggle_button):
        self.allow_send()

    def show_log_cb(self, widget, log):
        show_log(log, parent=self.window)

    # this callback is called when user press Cancel or Report button in Report dialog
    def on_response(self, dialog, response_id, daemon):
        # the button has been pressed (probably)
        if response_id == gtk.RESPONSE_APPLY:
            if not (self.check_report()):
                dialog.stop_emission("response")
                self.builder.get_object("bSend").stop_emission("clicked")
        if response_id == SHOW_LOG:
        # prevent the report dialog from quitting the run() and closing itself
            dialog.stop_emission("response")

    def on_send_toggled(self, cell, path, model):
        model[path][3] = not model[path][3]

    def on_comment_focus_cb(self, widget, event):
        if self.show_hint_comment:
            # clear "hint" text by supplying a fresh, empty TextBuffer
            widget.set_buffer(gtk.TextBuffer())
            self.show_hint_comment = 0

    def set_label(self, label_widget, text):
        if len(text) > label_widget.get_max_width_chars():
            label_widget.set_tooltip_text(text)
        label_widget.set_text(text)

    def hydrate(self):
        self.editable = []
        self.old_comment = COMMENT_HINT_TEXT
        self.old_how_to_reproduce = HOW_TO_HINT_TEXT
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
                #FIXME: temporary workaround, in 1.0.4 reporters don't care
                # about this, they just send what they want to
                # TicketUploader even sends coredump!!
                #toggle.show()
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
        self.show_hint_comment = (self.old_comment == COMMENT_HINT_TEXT)
        if self.show_hint_comment:
            buff.set_text(COMMENT_HINT_TEXT)
        else:
            buff.set_text(self.old_comment)
        self.tvComment.set_buffer(buff)

        buff = gtk.TextBuffer()
        if self.old_how_to_reproduce == "":
            buff.set_text(HOW_TO_HINT_TEXT)
        else:
            buff.set_text(self.old_how_to_reproduce)
        self.tevHowToReproduce.set_buffer(buff)

    def dehydrate(self):
        ## # handle attachments
        ## vbAttachments = self.builder.get_object("vbAttachments")
        ## for attachment in vbAttachments.get_children():
        ##     #print "%s file %s" % (["not sending","sending"][attachment.get_active()], attachment.get_label())
        ##     del self.report[attachment.item]

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
        # handle backtrace
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

class ReporterSelector():
    def __init__(self, crashdump, daemon, log=None, parent=None):
        self.connected_signals = []
        self.updates = ""
        self.daemon = daemon
        self.dump = crashdump
        self.selected_reporters = []
        #FIXME: cache settings! Create some class to represent it like PluginList
        self.settings = daemon.getSettings()
        pluginlist = getPluginInfoList(daemon)
        self.reporters = []
        AnalyzerActionsAndReporters = self.settings["AnalyzerActionsAndReporters"]
        try:
            reporters = None
            try:
                reporters = AnalyzerActionsAndReporters[self.dump.getAnalyzerName()+":"+self.dump.getPackageName()]
            except KeyError:
                pass
            if not reporters:
                reporters = AnalyzerActionsAndReporters[crashdump.getAnalyzerName()]
            for reporter_name in reporters.split(','):
                reporter = pluginlist.getReporterByName(reporter_name)
                if reporter:
                    self.reporters.append(reporter)
        except KeyError:
            # Analyzer has no associated reporters.
            pass

        builderfile = "%s/report.glade" % sys.path[0]
        self.builder = gtk.Builder()
        self.builder.add_from_file(builderfile)
        self.window = self.builder.get_object("w_reporters")
        b_cancel = self.builder.get_object("b_close")

        if parent:
            self.window.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.window.set_transient_for(parent)
            self.window.set_modal(True)
            self.connect_signal(self.window, "delete-event", self.on_window_delete)
            self.connect_signal(self.window, "destroy-event", self.on_window_delete)
            self.connect_signal(b_cancel, "clicked", self.on_close_clicked)
        else:
            # if we don't have parent we want to quit the mainloop on close
            self.window.set_position(gtk.WIN_POS_CENTER)
            self.connect_signal(self.window, "delete-event", gtk.main_quit)
            self.connect_signal(self.window, "destroy-event", gtk.main_quit)
            self.connect_signal(b_cancel, "clicked", gtk.main_quit)


        self.pBarWindow = self.builder.get_object("pBarWindow")
        self.pBarWindow.set_transient_for(self.window)

        reporters_vbox = self.builder.get_object("vb_reporters")
        for reporter in self.reporters:
            button = gtk.Button(str(reporter))
            self.connect_signal(button, "clicked", self.on_reporter_clicked, data=reporter)
            reporters_vbox.pack_start(button)

        # progress bar window to show while bt is being extracted
        self.pBarWindow = self.builder.get_object("pBarWindow")
        if self.pBarWindow:
            self.connect_signal(self.pBarWindow, "delete_event", self.sw_delete_event_cb)
            if parent:
                self.pBarWindow.set_transient_for(parent)
            else:
                self.pBarWindow.set_transient_for(self.window)
            self.pBar = self.builder.get_object("pBar")

        # connect handlers for daemon signals
        #self.ccdaemon.connect("abrt-error", self.error_cb)
        self.connect_signal(daemon, "update", self.update_cb)
        # for now, just treat them the same (w/o this, we don't even see daemon warnings in logs!):
        #self.ccdaemon.connect("warning", self.update_cb)
        #self.ccdaemon.connect("show", self.show_cb)
        #self.ccdaemon.connect("daemon-state-changed", self.on_daemon_state_changed_cb)
        self.connect_signal(daemon, "report-done", self.on_report_done_cb)
        self.connect_signal(daemon, "analyze-complete", self.on_analyze_complete_cb, self.pBarWindow)

    def connect_signal(self, obj, signal, callback, data=None):
        if data:
            signal_id = obj.connect(signal, callback, data)
        else:
            signal_id = obj.connect(signal, callback)
        self.connected_signals.append((obj, signal_id))

    def disconnect_signals(self):
    # we need to disconnect all signals in order to break all references
    # to this object, otherwise python won't destroy this object and the
    # signals emmited by daemon will get caught by multiple instances of
    # this class
        for obj, signal_id in self.connected_signals:
            obj.disconnect(signal_id)

    def cleanup_and_exit(self):
        if not self.window.get_property("visible"):
            self.disconnect_signals()
            # if the reporter selector doesn't have a parent
            if not self.window.get_transient_for():
                gtk.main_quit()

    def update_cb(self, daemon, message):
        self.updates += message
        if self.updates[-1] != '\n':
            self.updates += '\n'
        message = message.replace('\n',' ')
        self.builder.get_object("lStatus").set_text(message)
        buff = gtk.TextBuffer()
        buff.set_text(self.updates)
        end = buff.get_insert()
        tvUpdates = self.builder.get_object("tvUpdates")
        tvUpdates.set_buffer(buff)
        tvUpdates.scroll_mark_onscreen(end)

    def sw_delete_event_cb(self, widget, event, data=None):
        if self.timer:
            gobject.source_remove(self.timer)
        widget.hide()
        return True

    def show(self):
        if not self.reporters:
            gui_error_message(_("No reporter plugin available for this type of crash.\n"
                                 "Please check abrt.conf."))
        elif len(self.reporters) > 1:
            self.builder.get_object("vb_reporters").show_all()
            self.window.show()
        else:
            # we have only one reporter in the list
            self.selected_reporters = [str(self.reporters[0])]
            self.show_report()

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
                    gui_error_message(_("Cannot save plugin settings:\n %s" % e))
                box = image.get_parent()
                im = gtk.Image()
                im.set_from_stock(gtk.STOCK_APPLY, gtk.ICON_SIZE_MENU)
                box.remove(image)
                box.pack_start(im, expand = False, fill = False)
                im.show()
                image.destroy()
                button.set_sensitive(False)
        elif response == gtk.RESPONSE_CANCEL:
            log1("cancel")
        ui.destroy()

    def check_settings(self, reporters):
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
                button = gtk.Button(_("Configure %s options" % plugin.getName()))
                button.connect("clicked", self.on_config_plugin_clicked, plugin, image)
                hbox.pack_start(button)
                hbox.pack_start(image, expand = False, fill = False)
                vbWrongSettings.pack_start(hbox)
            vbWrongSettings.show_all()
            dialog.set_transient_for(self.window)
            dialog.set_modal(True)
            response = dialog.run()
            dialog.destroy()
            if response != gtk.RESPONSE_YES:
                # user cancelled reporting
                return False
        return True

    def on_reporter_clicked(self, widget, reporter):
        self.selected_reporters = [reporter]
        if self.check_settings(self.selected_reporters):
            self.show_report()

    def on_close_clicked(self, widget):
        self.disconnect_signals()
        self.window.destroy()

    def on_window_delete(self, window, event):
        self.disconnect_signals()
        return False

    def on_report_done_cb(self, daemon, result):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()
        gui_report_dialog(result, self.window)

        self.cleanup_and_exit()

    def on_analyze_complete_cb(self, daemon, report, pBarWindow):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()
#FIXME - why we need this?? -> timeout warnings
#        try:
#            dumplist = getDumpList(self.daemon)
#        except Exception, e:
#            print e
        if not report:
            gui_error_message(_("Unable to get report!\nIs debuginfo missing?"))
            return

        # if we have only one reporter enabled, the window with
        # the selection is not shown, so we can't use it as a parent
        # and we use the mainwindow instead
        if self.window.get_property("visible"):
            parent_window = self.window
        else:
            parent_window = self.window.get_transient_for()

        report_dialog = ReporterDialog(report, self.daemon, log=self.updates, parent=parent_window)
        # (response, report)
        response, result = report_dialog.run()

        if response == gtk.RESPONSE_APPLY:
            try:
                self.pBarWindow.show_all()
                self.timer = gobject.timeout_add(100, self.progress_update_cb)
                pluginlist = getPluginInfoList(self.daemon)
                reporters_settings = pluginlist.getReporterPluginsSettings()
                log2("Report(result,reporters,settings):")
                log2("  result:%s", str(result))
                # Careful, this will print reporters_settings["Password"] too
                log2("  settings:%s", str(reporters_settings))
                self.daemon.Report(result, self.selected_reporters, reporters_settings)
                log2("Report() returned")
                #self.hydrate()
            except Exception, ex:
                gui_error_message(_("Reporting failed!\n%s" % ex))
        # -50 == REFRESH
        elif response == -50:
            self.refresh_report(report)
        else:
            self.cleanup_and_exit()

    # call to update the progressbar
    def progress_update_cb(self, *args):
        self.pBar.pulse()
        return True

    def refresh_report(self, report):
        self.updates = ""
        self.pBarWindow.show_all()
        self.timer = gobject.timeout_add(100, self.progress_update_cb)

        # show the report window with selected report
        try:
            self.daemon.start_job("%s:%s" % (report[CD_UID][CD_CONTENT], report[CD_UUID][CD_CONTENT]), force=1)
        except Exception, ex:
            # FIXME #3  dbus.exceptions.DBusException: org.freedesktop.DBus.Error.NoReply: Did not receive a reply
            # do this async and wait for yum to end with debuginfoinstal
            if self.timer:
                gobject.source_remove(self.timer)
            self.pBarWindow.hide()
            gui_error_message(_("Error acquiring the report: %s" % ex))
        return

    def show_report(self):
        self.updates = ""
        # FIXME don't duplicate the code, move to function
        #self.pBar.show()
        self.pBarWindow.show_all()
        self.timer = gobject.timeout_add(100, self.progress_update_cb)

        # show the report window with selected dump
        # when getReport is done it emits "analyze-complete" and on_analyze_complete_cb is called
        # FIXME: does it make sense to change it to use callback rather then signal emitting?
        try:
            self.daemon.start_job("%s:%s" % (self.dump.getUID(), self.dump.getUUID()))
        except Exception, ex:
            # FIXME #3  dbus.exceptions.DBusException: org.freedesktop.DBus.Error.NoReply: Did not receive a reply
            # do this async and wait for yum to end with debuginfoinstal
            if self.timer:
                gobject.source_remove(self.timer)
            self.pBarWindow.hide()
            gui_error_message(_("Error acquiring the report: %s" % ex))
        return

    def __del__(self):
        log1("ReporterSelector: instance is about to be garbage-collected")
