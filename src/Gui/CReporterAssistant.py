import gtk
from PluginList import getPluginInfoList
from abrt_utils import _, log, log1, log2, get_verbose_level, g_verbose, warn
from CCDump import *   # FILENAME_xxx, CD_xxx
from PluginSettingsUI import PluginSettingsUI
import sys
import gobject
from CC_gui_functions import *
import pango

# assistant pages
PAGE_REPORTER_SELECTOR = 0
PAGE_BACKTRACE_APPROVAL = 1
PAGE_EXTRA_INFO = 2
PAGE_CONFIRM = 3
PAGE_REPORT_DONE = 4
NO_PROBLEMS_DETECTED = -50
HOW_TO_HINT_TEXT = "1.\n2.\n3.\n"
COMMENT_HINT_TEXT = _("Brief description of how to reproduce this or what you did...")
MISSING_BACKTRACE_TEXT = _("Crash info doesn't contain a backtrace")

DEFAULT_WIDTH = 800
DEFAULT_HEIGHT = 500

class ReporterAssistant():
    def __init__(self, report, daemon, log=None, parent=None):
        self.connected_signals = []
        self.plugins_cb = []
        self.daemon = daemon
        self.updates = ""
        self.pdict = {}
        self.report = report
        self.parent = parent
        self.comment_changed = False
        self.howto_changed = False
        self.report_has_bt = False
        self.selected_reporters = []
        """ create the assistant """
        self.assistant = gtk.Assistant()
        self.assistant.set_icon_name("abrt")
        self.assistant.set_default_size(DEFAULT_WIDTH,DEFAULT_HEIGHT)
        self.assistant.set_size_request(DEFAULT_WIDTH,DEFAULT_HEIGHT)
        self.connect_signal(self.assistant, "prepare",self.on_page_prepare)
        self.connect_signal(self.assistant, "cancel",self.on_cancel_clicked)
        self.connect_signal(self.assistant, "close",self.on_close_clicked)
        self.connect_signal(self.assistant, "apply",self.on_apply_clicked)
        if parent:
            self.assistant.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.assistant.set_transient_for(parent)
        else:
            # if we don't have parent we want to quit the mainloop on close
            self.assistant.set_position(gtk.WIN_POS_CENTER)

        ### progress bar window
        self.builder = gtk.Builder()
        builderfile = "%s/progress_window.glade" % sys.path[0]
        self.builder.add_from_file(builderfile)
        self.pBarWindow = self.builder.get_object("pBarWindow")
        if self.pBarWindow:
            self.connect_signal(self.pBarWindow, "delete_event", self.sw_delete_event_cb)
            if parent:
                self.pBarWindow.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
                self.pBarWindow.set_transient_for(parent)
            else:
                self.pBarWindow.set_position(gtk.WIN_POS_CENTER)
            self.pBar = self.builder.get_object("pBar")
        else:
            log1("Couldn't create the progressbar window")

        self.connect_signal(daemon, "analyze-complete", self.on_analyze_complete_cb, self.pBarWindow)
        self.connect_signal(daemon, "report-done", self.on_report_done_cb)
        self.connect_signal(daemon, "update", self.update_cb)

    # call to update the progressbar
    def progress_update_cb(self, *args):
        self.pBar.pulse()
        return True

    def on_show_bt_clicked(self, button):
        viewer = gtk.Window()
        viewer.set_icon_name("abrt")
        viewer.set_default_size(600,500)
        viewer.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
        viewer.set_transient_for(self.assistant)
        vbox = gtk.VBox()
        viewer.add(vbox)
        bt_tev = gtk.TextView()
        backtrace_scroll_w = gtk.ScrolledWindow()
        backtrace_scroll_w.add(bt_tev)
        backtrace_scroll_w.set_policy(gtk.POLICY_AUTOMATIC,
                                      gtk.POLICY_AUTOMATIC)
        bt_tev.set_buffer(self.backtrace_buff)
        vbox.pack_start(backtrace_scroll_w)
        b_close = gtk.Button(stock=gtk.STOCK_CLOSE)
        b_close.connect("clicked",lambda *w: viewer.destroy())
        vbox.pack_start(b_close, False)
        viewer.show_all()

    def on_report_done_cb(self, daemon, result):
        self.hide_progress()
        STATUS = 0
        MESSAGE = 1
        # 0 means not succesfull
        #if report_status_dict[plugin][STATUS] == '0':
        # this first one is actually a fallback to set at least
        # a raw text in case when set_markup() fails
        for plugin, res in result.iteritems():
            bug_report = gtk.Label()
            bug_report.set_selectable(True)
            bug_report.set_line_wrap(True)
            bug_report.set_alignment(0.0, 0.0)
            bug_report.set_justify(gtk.JUSTIFY_LEFT)
            bug_report.set_size_request(DEFAULT_WIDTH-50, -1)
            bug_report.set_text(result[plugin][MESSAGE])
            bug_report.set_markup("<span foreground='red'>%s</span>" % markup_escape_text(result[plugin][MESSAGE]))
            # if the report was not succesful then this won't pass so this runs only
            # if report succeds and gets overwriten by the status message
            if result[plugin][STATUS] == '1':
                bug_report.set_markup(tag_urls_in_text(result[plugin][MESSAGE]))
            self.bug_reports_vbox.pack_start(bug_report, expand=False)
            bug_report.show()


            #if len(result[plugin][1]) > MAX_WIDTH:
            #    self.bug_reports.set_tooltip_text(result[plugin][1])
            #gui_report_dialog(result, self.parent)

    def cleanup_and_exit(self):
        self.disconnect_signals()
        self.assistant.destroy()
        if not self.parent:
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

    def connect_signal(self, obj, signal, callback, data=None):
        if data:
            signal_id = obj.connect(signal, callback, data)
        else:
            signal_id = obj.connect(signal, callback)
        log1("connected signal %s:%s" % (signal, signal_id))
        self.connected_signals.append((obj, signal_id))

    def disconnect_signals(self):
    # we need to disconnect all signals in order to break all references
    # to this object, otherwise python won't destroy this object and the
    # signals emmited by daemon will get caught by multiple instances of
    # this class
        for obj, signal_id in self.connected_signals:
            log1("disconnect %s:%s" % (obj, signal_id))
            obj.disconnect(signal_id)

    def on_cancel_clicked(self, assistant, user_data=None):
        self.cleanup_and_exit()

    def on_close_clicked(self, assistant, user_data=None):
        self.cleanup_and_exit()

    def on_apply_clicked(self, assistant, user_data=None):
        self.send_report(self.result)

    def hide_progress(self):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()

    def on_config_plugin_clicked(self, button, parent, plugin, image):
        try:
            ui = PluginSettingsUI(plugin, parent=parent)
        except Exception, ex:
            gui_error_message(str(ex))
            return

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
                button.connect("clicked", self.on_config_plugin_clicked, dialog, plugin, image)
                hbox.pack_start(button)
                hbox.pack_start(image, expand = False, fill = False)
                vbWrongSettings.pack_start(hbox)
            vbWrongSettings.show_all()
            dialog.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            dialog.set_transient_for(self.assistant)
            dialog.set_modal(True)
            response = dialog.run()
            dialog.destroy()
            if response == gtk.RESPONSE_YES:
                return True
            else:
                # user cancelled reporting
                return False
        else:
            return NO_PROBLEMS_DETECTED

    def warn_user(self, warnings):
        # FIXME: show in lError
        #self.lErrors = self.builder.get_object("lErrors")
        warning_lbl = None
        for warning in warnings:
            if warning_lbl:
                warning_lbl += "\n* %s" % warning
            else:
                warning_lbl = "* %s" % warning
        # fallback
        self.lbl_errors.set_label(warning_lbl)
        self.lbl_errors.set_markup(warning_lbl)
        self.errors_hbox.show_all()
        #fErrors.show_all()

    def hide_warning(self):
        self.errors_hbox.hide()

    def allow_send(self, send_toggle):
        self.hide_warning()
        #bSend = self.builder.get_object("bSend")
        SendBacktrace = send_toggle.get_active()
        send = True
        error_msgs = []
        rating_required = False

        for reporter in self.selected_reporters:
            if "RatingRequired" in reporter.Settings.keys():
                if reporter.Settings["RatingRequired"] == "yes":
                    rating_required = True
                    log1(_("Rating is required by the %s plugin") % reporter)
        if self.selected_reporters and not rating_required:
            log1(_("Rating is not required by any plugin, skipping the check..."))

        try:
            rating = int(self.result[FILENAME_RATING][CD_CONTENT])
            log1(_("Rating is %s" % rating))
        except Exception, ex:
            rating = None
            log1(_("Crashdump doesn't have rating => we suppose it's not required"))
        # active buttons acording to required fields
        # if an backtrace has rating use it
        if not SendBacktrace:
            send = False
            error_msgs.append(_("You should check the backtrace for sensitive data."))
            error_msgs.append(_("You must agree with sending the backtrace."))
        # we have both SendBacktrace and rating
        # if analyzer doesn't provide the rating, then we suppose that it's
        # not required e.g.: kerneloops, python
        if rating_required and rating != None:
            try:
                package = self.result[FILENAME_PACKAGE][CD_CONTENT]
            # if we don't have package for some reason
            except:
                package = None
            # not usable report
            if rating < 3:
                if package:
                    error_msgs.append(_("Reporting disabled because the backtrace is unusable.\nPlease try to install debuginfo manually using the command: <b>debuginfo-install %s</b> \nthen use the Refresh button to regenerate the backtrace." % self.report.getPackageName()))
                else:
                    error_msgs.append(_("Reporting disabled because the backtrace is unusable."))
                send = False
            # probably usable 3
            elif rating < 4:
                error_msgs.append(_("The backtrace is incomplete, please make sure you provide the steps to reproduce."))

        if error_msgs:
            self.warn_user(error_msgs)
        #bSend.set_sensitive(send)
        self.assistant.set_page_complete(self.pdict_get_page(PAGE_BACKTRACE_APPROVAL), send)

    def on_page_prepare(self, assistant, page):
        if page == self.pdict_get_page(PAGE_REPORTER_SELECTOR):
            pass

        # this is where dehydrate happens
        elif page == self.pdict_get_page(PAGE_EXTRA_INFO):
            if not self.howto_changed:
                # howto
                buff = gtk.TextBuffer()
                try:
                    buff.set_text(self.result[FILENAME_REPRODUCE][CD_CONTENT])
                    self.howto_changed = True
                except KeyError:
                    buff.set_text(HOW_TO_HINT_TEXT)
                self.howto_tev.set_buffer(buff)
            # don't refresh the comment if user changed it
            if not self.comment_changed:
                # comment
                buff = gtk.TextBuffer()
                try:
                    buff.set_text(self.result[FILENAME_COMMENT][CD_CONTENT])
                    self.comment_changed = True
                except KeyError:
                    buff.set_text(COMMENT_HINT_TEXT)
                self.comment_tev.set_buffer(buff)
        elif page == self.pdict_get_page(PAGE_CONFIRM):
            # howto
            if self.howto_changed:
                howto_buff = self.howto_tev.get_buffer()
                howto_text = howto_buff.get_text(howto_buff.get_start_iter(), howto_buff.get_end_iter())
                # user has changed the steps to reproduce
                self.steps.set_text(howto_text)
                self.result[FILENAME_REPRODUCE] = [CD_TXT, 'y', howto_text]
            else:
                self.steps.set_text(_("You did not provide any steps to reproduce."))
                try:
                    del self.result[FILENAME_REPRODUCE]
                except KeyError:
                    # if this is a first time, then we don't have key FILENAME_REPRODUCE
                    pass
            #comment
            if self.comment_changed:
                comment_buff = self.comment_tev.get_buffer()
                comment_text = comment_buff.get_text(comment_buff.get_start_iter(), comment_buff.get_end_iter())
                # user has changed the comment
                self.comments.set_text(comment_text)
                self.result[FILENAME_COMMENT] = [CD_TXT, 'y', comment_text]
            else:
                self.comments.set_text(_("You did not provide any comments."))
                try:
                    del self.result[FILENAME_COMMENT]
                except KeyError:
                    # if this is a first time, then we don't have key FILENAME_COMMENT
                    pass

            # backtrace
            backtrace_text = self.backtrace_buff.get_text(self.backtrace_buff.get_start_iter(), self.backtrace_buff.get_end_iter())
            if self.report_has_bt:
                self.result[FILENAME_BACKTRACE] = [CD_TXT, 'y', backtrace_text]
        if page == self.pdict_get_page(PAGE_BACKTRACE_APPROVAL):
            self.allow_send(self.backtrace_cb)

    def send_report(self, report):
        try:
            self.pBarWindow.show_all()
            self.timer = gobject.timeout_add(100, self.progress_update_cb)
            pluginlist = getPluginInfoList(self.daemon)
            reporters_settings = pluginlist.getReporterPluginsSettings()
            log2("Report(report, reporters, settings):")
            log2("  result:%s", str(report))
            # Careful, this will print reporters_settings["Password"] too
            log2("  settings:%s", str(reporters_settings))
            self.daemon.Report(report, self.selected_reporters, reporters_settings)
            log2("Report() returned")
            #self.hydrate()
        except Exception, ex:
            self.hide_progress()
            gui_error_message(_("Reporting failed!\n%s" % ex))

    def on_plugin_toggled(self, plugin, plugins, reporter, page):
        complete = False
        if plugin.get_active():
            log1("Plugin >>%s<< activated" % reporter)
            self.selected_reporters.append(reporter)
            check_result = self.check_settings([reporter])
            if check_result == NO_PROBLEMS_DETECTED:
                pass
            elif check_result:
                page_n = self.assistant.get_current_page()
                self.assistant.set_page_complete(page, True)
                self.assistant.set_current_page(page_n+1)
            else:
                plugin.set_active(False)
        else:
            self.selected_reporters.remove(reporter)
            log1("Plugin >>%s<< de-activated" % reporter)
        if self.selected_reporters:
            complete = True
        log1("Selected reporters: %s" % [str(x) for x in self.selected_reporters])
        self.assistant.set_page_complete(page, complete)

    def on_bt_toggled(self, togglebutton, page):
        self.allow_send(togglebutton)

    def pdict_add_page(self, page, name):
        # FIXME try, except??
        if name not in self.pdict:
            self.pdict[name] = page
        else:
            warn("The page %s is already in the dictionary" % name)
            #raise Exception("The page %s is already in the dictionary" % name)

    def pdict_get_page(self, name):
        try:
            return self.pdict[name]
        except Exception, e:
            log2(e)
            return None

    def prepare_page_1(self):
        plugins_cb = []
        page = gtk.VBox(spacing=10)
        page.set_border_width(10)
        self.assistant.insert_page(page, PAGE_REPORTER_SELECTOR)
        lbl_default_info = gtk.Label()
        lbl_default_info.set_line_wrap(True)
        lbl_default_info.set_alignment(0.0, 0.0)
        lbl_default_info.set_justify(gtk.JUSTIFY_LEFT)
        lbl_default_info.set_size_request(DEFAULT_WIDTH-50, -1)
        lbl_default_info.set_markup(_("It looks like an application from the "
                                    "package <b>%s</b> has crashed "
                                    "on your system. It is a good idea to send "
                                    "a bug report about this issue. The report "
                                    "will provide software maintainers with "
                                    "information essential in figuring out how "
                                    "to provide a bug fix for you.\n\n"
                                    "Please review the information that follows "
                                    "and modify it as needed to ensure your bug "
                                    "report does not contain any sensitive data "
                                    "you would rather not share.\n\n"
                                    "Select where you would like to report the "
                                    "bug, and press 'Forward' to continue.")
                                    % self.report.getPackageName())
        page.pack_start(lbl_default_info, expand=True, fill=True)
        vbox_plugins = gtk.VBox()
        page.pack_start(vbox_plugins)

        # add checkboxes for enabled reporters
        self.selected_reporters = []
        #FIXME: cache settings! Create some class to represent it like PluginList
        self.settings = self.daemon.getSettings()
        pluginlist = getPluginInfoList(self.daemon)
        self.reporters = []
        AnalyzerActionsAndReporters = self.settings["AnalyzerActionsAndReporters"]
        try:
            reporters = None
            try:
                reporters = AnalyzerActionsAndReporters[self.report.getAnalyzerName()+":"+self.report.getPackageName()]
                log1("Found per-package reporters, "
                     "using it instead of the common reporter")
            except KeyError:
                pass
            # the package specific reporter has higher priority,
            # so don't overwrite it if it's set
            if not reporters:
                reporters = AnalyzerActionsAndReporters[self.report.getAnalyzerName()]
            # FIXME: split(',') doesn't work for RunApp("date", "date.txt")
            # but since we don't have reporters with parameters, it will find
            # the reporter plugins anyway, but it should be more clever...
            for reporter_name in reporters.split(','):
                reporter = pluginlist.getReporterByName(reporter_name)
                if reporter:
                    log1("Adding >>%s<< to reporters", reporter)
                    self.reporters.append(reporter)
        except KeyError:
            # Analyzer has no associated reporters.
            # but we don't care, maybe user just want to read the backtrace??
            pass
        for reporter in self.reporters:
            cb = gtk.CheckButton(str(reporter))
            cb.connect("toggled", self.on_plugin_toggled, plugins_cb, reporter, page)
            plugins_cb.append(cb)
            vbox_plugins.pack_start(cb, fill=True, expand=False)
        # automatically select the reporter if we have only one reporter plugin
        if len(self.reporters) == 1:
            # we want to skip it only if the plugin is properly configured
            if self.reporters[0].Settings.check():
                #self.selected_reporters.append(self.reporters[0])
                self.assistant.set_page_complete(page, True)
                log1(_("Only one reporter plugin is configured."))
                # this is safe, because in python the variable is visible even
                # outside the for loop
                cb.set_active(True)
        self.pdict_add_page(page, PAGE_REPORTER_SELECTOR)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_INTRO)
        self.assistant.set_page_title(page, _("Send a bug report"))
        page.show_all()

    def on_bt_copy(self, button, bt_text_view):
        buff = bt_text_view.get_buffer()
        bt_text = buff.get_text(buff.get_start_iter(), buff.get_end_iter())
        clipboard = gtk.clipboard_get()
        clipboard.set_text(bt_text)

    def tv_text_changed(self, textview, default_text):
        buff = textview.get_buffer()
        text = buff.get_text(buff.get_start_iter(), buff.get_end_iter())
        if text:
            if text and text != default_text:
                return True
            return False
        else:
            buff.set_text(default_text)

    def on_howto_focusout_cb(self, textview, event):
        self.howto_changed = self.tv_text_changed(textview, HOW_TO_HINT_TEXT)

    def on_comment_focusin_cb(self, textview, event):
        if not self.comment_changed:
            # clear "hint" text by supplying a fresh, empty TextBuffer
            textview.set_buffer(gtk.TextBuffer())

    def on_comment_focusout_cb(self, textview, event):
        self.comment_changed = self.tv_text_changed(textview, COMMENT_HINT_TEXT)

    def prepare_page_2(self):
        page = gtk.VBox(spacing=10)
        page.set_border_width(10)
        lbl_default_info = gtk.Label()
        lbl_default_info.set_line_wrap(True)
        lbl_default_info.set_alignment(0.0, 0.0)
        lbl_default_info.set_justify(gtk.JUSTIFY_FILL)
        lbl_default_info.set_size_request(DEFAULT_WIDTH-50, -1)
        lbl_default_info.set_text(_("Below is the backtrace associated with your "
            "crash. A crash backtrace provides developers with details about "
            "how the crash happened, helping them track down the source of the "
            "problem.\n\n"
            "Please review the backtrace below and modify it as needed to "
            "ensure your bug report does not contain any sensitive data you would "
            "rather not share:")
            )
        page.pack_start(lbl_default_info, expand=False, fill=True)
        self.backtrace_tev = gtk.TextView()
        # global?
        self.backtrace_buff = gtk.TextBuffer()
        #self.backtrace_buff.set_text(self.report[FILENAME_BACKTRACE][CD_CONTENT])
        self.backtrace_tev.set_buffer(self.backtrace_buff)
        backtrace_scroll_w = gtk.ScrolledWindow()
        backtrace_scroll_w.add(self.backtrace_tev)
        backtrace_scroll_w.set_policy(gtk.POLICY_AUTOMATIC,
                                      gtk.POLICY_AUTOMATIC)
        # backtrace
        hbox_bt = gtk.HBox()
        vbox_bt = gtk.VBox(homogeneous=False, spacing=5)
        hbox_bt.pack_start(vbox_bt)
        backtrace_alignment = gtk.Alignment()
        hbox_bt.pack_start(backtrace_alignment, expand=False, padding=10)
        # bad backtrace, reporting disabled
        vbox_bt.pack_start(backtrace_scroll_w)
        # warnings about wrong bt
        self.errors_hbox = gtk.HBox()
        self.warning_image = gtk.Image()
        self.warning_image.set_from_stock(gtk.STOCK_DIALOG_WARNING,gtk.ICON_SIZE_DIALOG)
        self.lbl_errors = gtk.Label()
        self.lbl_errors.set_line_wrap(True)
        #self.lbl_errors.set_alignment(0.0, 0.0)
        self.lbl_errors.set_justify(gtk.JUSTIFY_FILL)
        self.lbl_errors.set_size_request(DEFAULT_WIDTH-50, -1)
        self.errors_hbox.pack_start(self.warning_image, False, False)
        self.errors_hbox.pack_start(self.lbl_errors)
        ###
        vbox_bt.pack_start(self.errors_hbox, False, False)
        hbox_buttons = gtk.HBox(homogeneous=True)
        button_alignment = gtk.Alignment()
        b_refresh = gtk.Button(_("Refresh"))
        b_refresh.connect("clicked", self.hydrate, 1)
        b_copy = gtk.Button(_("Copy"))
        b_copy.connect("clicked", self.on_bt_copy, self.backtrace_tev)
        hbox_buttons.pack_start(button_alignment)
        hbox_buttons.pack_start(b_refresh, expand=False, fill=True)
        hbox_buttons.pack_start(b_copy, expand=False, fill=True)
        vbox_bt.pack_start(hbox_buttons, expand=False, fill=False)
        self.backtrace_cb = gtk.CheckButton(_("I agree with submitting the backtrace"))
        self.backtrace_cb.connect("toggled", self.on_bt_toggled, page)
        self.assistant.insert_page(page, PAGE_BACKTRACE_APPROVAL)
        self.pdict_add_page(page, PAGE_BACKTRACE_APPROVAL)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_CONTENT)
        self.assistant.set_page_title(page, _("Approve the backtrace"))
        page.pack_start(hbox_bt)
        page.pack_start(self.backtrace_cb, expand=False, fill=False)
        page.show_all()

    def prepare_page_3(self):
        page = gtk.VBox(spacing=10)
        page.set_border_width(10)
        #lbl_default_info = gtk.Label()
        #lbl_default_info.set_line_wrap(True)
        #lbl_default_info.set_alignment(0.0, 0.0)
        #lbl_default_info.set_justify(gtk.JUSTIFY_FILL)
        #lbl_default_info.set_size_request(600, -1)
        #page.pack_start(lbl_default_info, expand=False, fill=True)
        details_hbox = gtk.HBox()
        details_hbox.set_border_width(10)
        details_alignment = gtk.Alignment()
        details_vbox = gtk.VBox(spacing=10)
        details_hbox.pack_start(details_vbox)
        details_hbox.pack_start(details_alignment, expand=False, padding=30)

        # how to reproduce
        howto_vbox = gtk.VBox(spacing=5)
        howto_lbl = gtk.Label(_("How did this crash happen (step-by-step)? "
                           "How would you reproduce it?"))
        howto_lbl.set_alignment(0.0, 0.0)
        howto_lbl.set_justify(gtk.JUSTIFY_FILL)
        self.howto_tev = gtk.TextView()
        self.howto_tev.set_accepts_tab(False)
        self.howto_tev.connect("focus-out-event", self.on_howto_focusout_cb)
        howto_buff = gtk.TextBuffer()
        howto_buff.set_text(HOW_TO_HINT_TEXT)
        self.howto_tev.set_buffer(howto_buff)
        howto_scroll_w = gtk.ScrolledWindow()
        howto_scroll_w.add(self.howto_tev)
        howto_scroll_w.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        howto_vbox.pack_start(howto_lbl, expand=False, fill=True)
        howto_vbox.pack_start(howto_scroll_w)

        # comment
        comment_vbox = gtk.VBox(spacing=5)
        comment_lbl = gtk.Label(_("Are there any comments you would like to share "
            "with the software maintainers?"))
        comment_lbl.set_alignment(0.0, 0.0)
        comment_lbl.set_justify(gtk.JUSTIFY_FILL)
        self.comment_tev = gtk.TextView()
        self.comment_tev.set_accepts_tab(False)
        self.comment_tev.connect("focus-in-event", self.on_comment_focusin_cb)
        self.comment_tev.connect("focus-out-event", self.on_comment_focusout_cb)
        comment_scroll_w = gtk.ScrolledWindow()
        comment_scroll_w.add(self.comment_tev)
        comment_scroll_w.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        comment_vbox.pack_start(comment_lbl, expand=False, fill=True)
        comment_vbox.pack_start(comment_scroll_w)

        details_vbox.pack_start(howto_vbox)
        details_vbox.pack_start(comment_vbox)
        self.assistant.insert_page(page, PAGE_EXTRA_INFO)
        self.pdict_add_page(page, PAGE_EXTRA_INFO)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_CONTENT)
        self.assistant.set_page_title(page, _("Provide additional details"))
        self.assistant.set_page_complete(page, True)
        tip_hbox = gtk.HBox()
        tip_image = gtk.Image()
        tip_lbl = gtk.Label("")
        tip_lbl.set_alignment(0.0, 0.0)
        tip_lbl.set_justify(gtk.JUSTIFY_FILL)
        tip_lbl.set_markup(_("<b>Tip:</b> Your comments are not private. "
            "Please watch what you say accordingly."))
        #tip_hbox.pack_start(tip_image)
        tip_hbox.pack_start(tip_lbl, expand=False)
        page.pack_start(details_hbox)
        tip_alignment = gtk.Alignment()
        #page.pack_start(tip_alignment, padding=10)
        page.pack_start(tip_hbox, expand=False)
        page.show_all()

    def prepare_page_4(self):
        lines_in_table = {}
        width, height = self.assistant.get_size()
        def add_info_to_table(table, heading, text):
            line = 0
            if table in lines_in_table:
                line = lines_in_table[table]

            heading_lbl = gtk.Label()
            heading_lbl.set_alignment(0.0, 0.0)
            heading_lbl.set_justify(gtk.JUSTIFY_LEFT)
            heading_lbl.set_markup("<b>%s:</b>" % heading)
            table.attach(heading_lbl, 0, 1, line, line+1,
                xoptions=gtk.FILL, yoptions=gtk.EXPAND|gtk.FILL,
                xpadding=5, ypadding=5)
            lbl = gtk.Label(text)
            lbl.set_line_wrap(True)
            lbl.set_size_request(width/4, -1)
            lbl.set_alignment(0.0, 0.0)
            lbl.set_justify(gtk.JUSTIFY_LEFT)
            table.attach(lbl, 1, 2, line, line+1,
                xoptions=gtk.FILL, yoptions=gtk.EXPAND|gtk.FILL,
                xpadding=5, ypadding=5)

            lines_in_table[table] = line+1

        page = gtk.VBox(spacing=20)
        page.set_border_width(10)
        self.assistant.insert_page(page, PAGE_CONFIRM)
        self.pdict_add_page(page, PAGE_CONFIRM)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_CONFIRM)
        self.assistant.set_page_title(page, _("Confirm and send the report"))
        self.assistant.set_page_complete(page, True)
        summary_lbl = gtk.Label(_("Below is a summary of your bug report. "
            "Please click 'Apply' to submit it."))
        summary_lbl.set_alignment(0.0, 0.0)
        summary_lbl.set_justify(gtk.JUSTIFY_FILL)
        basic_details_lbl = gtk.Label()
        basic_details_lbl.set_markup(_("<b>Basic details</b>"))
        basic_details_lbl.set_alignment(0.0, 0.0)
        basic_details_lbl.set_justify(gtk.JUSTIFY_FILL)

        summary_table_left = gtk.Table(rows=4, columns=2)
        summary_table_right = gtk.Table(rows=4, columns=2)
        # left table
        add_info_to_table(summary_table_left, _("Component"), "%s" % self.report.get_component())
        add_info_to_table(summary_table_left, _("Package"), "%s" % self.report.getPackageName())
        add_info_to_table(summary_table_left, _("Executable"), "%s" % self.report.getExecutable())
        add_info_to_table(summary_table_left, _("Cmdline"), "%s" % self.report.get_cmdline())
        #right table
        add_info_to_table(summary_table_right, _("Architecture"), "%s" % self.report.get_arch())
        add_info_to_table(summary_table_right, _("Kernel"), "%s" % self.report.get_kernel())
        add_info_to_table(summary_table_right, _("Release"),"%s" % self.report.get_release())
        add_info_to_table(summary_table_right, _("Reason"), "%s" % self.report.get_reason())

        summary_hbox = gtk.HBox(spacing=5, homogeneous=True)
        left_table_vbox = gtk.VBox()
        left_table_vbox.pack_start(summary_table_left, expand=False, fill=False)
        left_table_vbox.pack_start(gtk.Alignment())
        summary_hbox.pack_start(left_table_vbox, expand=False, fill=True)
        summary_hbox.pack_start(summary_table_right, expand=False, fill=True)

        # backtrace
        backtrace_lbl = gtk.Label()
        backtrace_lbl.set_markup(_("<b>Backtrace</b>"))
        backtrace_lbl.set_alignment(0.0, 0.5)
        backtrace_lbl.set_justify(gtk.JUSTIFY_LEFT)
        backtrace_show_btn = gtk.Button(_("Click to view..."))
        backtrace_show_btn.connect("clicked", self.on_show_bt_clicked)
        backtrace_hbox = gtk.HBox(homogeneous=True)
        hb = gtk.HBox()
        hb.pack_start(backtrace_lbl)
        hb.pack_start(backtrace_show_btn, expand=False)
        backtrace_hbox.pack_start(hb)
        alignment = gtk.Alignment()
        backtrace_hbox.pack_start(alignment)

        # steps to reporoduce
        reproduce_lbl = gtk.Label()
        reproduce_lbl.set_markup(_("<b>Steps to reproduce:</b>"))
        reproduce_lbl.set_alignment(0.0, 0.0)
        reproduce_lbl.set_justify(gtk.JUSTIFY_LEFT)
        self.steps = gtk.Label()
        self.steps.set_alignment(0.0, 0.0)
        self.steps.set_justify(gtk.JUSTIFY_LEFT)
        self.steps.set_line_wrap(True)
        self.steps.set_line_wrap_mode(pango.WRAP_CHAR)
        self.steps.set_size_request(int(DEFAULT_WIDTH*0.8), -1)
        #self.steps_lbl.set_text("1. Fill in information about step 1.\n"
        #                   "2. Fill in information about step 2.\n"
        #                   "3. Fill in information about step 3.\n")
        steps_aligned_hbox = gtk.HBox()
        self.steps_hbox = gtk.HBox(spacing=10)
        self.steps_hbox.pack_start(reproduce_lbl)
        self.steps_hbox.pack_start(self.steps)
        steps_aligned_hbox.pack_start(self.steps_hbox, expand=False)
        steps_aligned_hbox.pack_start(gtk.Alignment())

        # comments
        comments_lbl = gtk.Label()
        comments_lbl.set_markup(_("<b>Comments:</b>"))
        comments_lbl.set_alignment(0.0, 0.0)
        comments_lbl.set_justify(gtk.JUSTIFY_LEFT)
        self.comments = gtk.Label(_("No comment provided!"))
        self.comments.set_line_wrap(True)
        self.comments.set_line_wrap_mode(pango.WRAP_CHAR)
        self.comments.set_size_request(int(DEFAULT_WIDTH*0.8), -1)
        comments_hbox = gtk.HBox(spacing=10)
        comments_hbox.pack_start(comments_lbl)
        comments_hbox.pack_start(self.comments)
        comments_aligned_hbox = gtk.HBox()
        comments_aligned_hbox.pack_start(comments_hbox, expand=False)
        comments_aligned_hbox.pack_start(gtk.Alignment())

        # pack all into the page

        summary_vbox = gtk.VBox(spacing=20)
        summary_vbox.pack_start(summary_hbox, expand=False)
        summary_vbox.pack_start(backtrace_hbox, expand=False)
        summary_vbox.pack_start(steps_aligned_hbox, expand=False)
        summary_vbox.pack_start(comments_aligned_hbox, expand=False)
        summary_scroll = gtk.ScrolledWindow()
        summary_scroll.set_shadow_type(gtk.SHADOW_NONE)
        summary_scroll.set_policy(gtk.POLICY_NEVER, gtk.POLICY_ALWAYS)
        scroll_viewport = gtk.Viewport()
        scroll_viewport.set_shadow_type(gtk.SHADOW_NONE)
        scroll_viewport.add(summary_vbox)
        summary_scroll.add(scroll_viewport)
        page.pack_start(summary_lbl, expand=False)
        page.pack_start(basic_details_lbl, expand=False)
        page.pack_start(summary_scroll)
        page.show_all()

    def prepare_page_5(self):
        page = gtk.VBox(spacing=20)
        page.set_border_width(10)
        self.assistant.insert_page(page, PAGE_REPORT_DONE)
        self.pdict_add_page(page, PAGE_REPORT_DONE)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_SUMMARY)
        self.assistant.set_page_title(page, _("Finished sending the bug report"))
        bug_reports_lbl = gtk.Label()
        bug_reports_lbl.set_alignment(0.0, 0.0)
        bug_reports_lbl.set_justify(gtk.JUSTIFY_LEFT)
        bug_reports_lbl.set_markup(_("<b>Bug reports:</b>"))
        width, height = self.assistant.get_size()
        self.bug_reports_vbox = gtk.VBox(spacing=5)
        self.bug_reports_vbox.pack_start(bug_reports_lbl, expand=False)
        page.pack_start(self.bug_reports_vbox)
        page.show_all()

    def __del__(self):
        log1("wizard: about to be deleted")

    def on_analyze_complete_cb(self, daemon, result, pBarWindow):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()
        if not result:
            gui_error_message(_("Unable to get report!\nIs debuginfo missing?"))
            return
        self.result = result
        # set the backtrace text
        try:
            self.backtrace_buff.set_text(self.result[FILENAME_BACKTRACE][CD_CONTENT])
            self.report_has_bt = True
        except:
            self.backtrace_buff.set_text(MISSING_BACKTRACE_TEXT)
            self.backtrace_cb.set_active(True)
            log1("Crash info doesn't contain a backtrace, is it disabled?")

        self.allow_send(self.backtrace_cb)
        self.show()

    def hydrate(self, button=None, force=0):
        if not force:
            self.prepare_page_1()
            self.prepare_page_2()
            self.prepare_page_3()
            self.prepare_page_4()
            self.prepare_page_5()
        self.updates = ""
        # FIXME don't duplicate the code, move to function
        #self.pBar.show()
        self.pBarWindow.show_all()
        self.timer = gobject.timeout_add(100, self.progress_update_cb)

        # show the report window with selected report
        # when getReport is done it emits "analyze-complete" and on_analyze_complete_cb is called
        # FIXME: does it make sense to change it to use callback rather then signal emitting?
        try:
            self.daemon.start_job("%s:%s" % (self.report.getUID(), self.report.getUUID()), force)
        except Exception, ex:
            # FIXME #3  dbus.exceptions.DBusException: org.freedesktop.DBus.Error.NoReply: Did not receive a reply
            # do this async and wait for yum to end with debuginfoinstal
            if self.timer:
                gobject.source_remove(self.timer)
            self.pBarWindow.hide()
            gui_error_message(_("Error acquiring the report: %s" % ex))
            return

    def show(self):
        self.assistant.show()


if __name__ == "__main__":
    wiz = ReporterAssistant()
    wiz.show()
    gtk.main()
