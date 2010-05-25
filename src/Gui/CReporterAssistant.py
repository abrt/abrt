import gtk
from PluginList import getPluginInfoList
from abrt_utils import _, log, log1, log2, get_verbose_level, g_verbose, warn
from CCDump import *   # FILENAME_xxx, CD_xxx
import sys
import gobject
from CC_gui_functions import *

# assistant pages
PAGE_REPORTER_SELECTOR = 0
PAGE_BACKTRACE_APPROVAL = 1
PAGE_EXTRA_INFO = 2
PAGE_CONFIRM = 3
PAGE_REPORT_DONE = 4
HOW_TO_HINT_TEXT = "1.\n2.\n3.\n"

class ReporterAssistant():
    def __init__(self, dump, daemon, log=None, parent=None):
        self.report = None
        self.connected_signals = []
        self.plugins_cb = []
        self.daemon = daemon
        self.updates = ""
        self.pdict = {}
        self.dump = dump
        self.parent = parent
        """ create the assistant """
        self.assistant = gtk.Assistant()
        self.assistant.set_icon_name("abrt")
        self.assistant.set_default_size(600,500)
        self.connect_signal(self.assistant, "prepare",self.on_page_prepare)
        self.connect_signal(self.assistant, "cancel",self.on_cancel_clicked)
        self.connect_signal(self.assistant, "close",self.on_close_clicked)
        self.connect_signal(self.assistant, "apply",self.on_apply_clicked)
        if parent:
            self.assistant.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.assistant.set_transient_for(parent)

        ### progress bar window
        self.builder = gtk.Builder()
        builderfile = "%s/progress_window.glade" % sys.path[0]
        self.builder.add_from_file(builderfile)
        self.pBarWindow = self.builder.get_object("pBarWindow")
        print self.pBarWindow
        if self.pBarWindow:
            self.connect_signal(self.pBarWindow, "delete_event", self.sw_delete_event_cb)
            if parent:
                self.pBarWindow.set_transient_for(parent)
            #else:
            #    self.pBarWindow.set_transient_for(self.window)
            self.pBar = self.builder.get_object("pBar")
        else:
            print "oops"

        self.connect_signal(daemon, "analyze-complete", self.on_analyze_complete_cb, self.pBarWindow)
        self.connect_signal(daemon, "report-done", self.on_report_done_cb)
        self.connect_signal(daemon, "update", self.update_cb)

    # call to update the progressbar
    def progress_update_cb(self, *args):
        self.pBar.pulse()
        return True

    def on_report_done_cb(self, daemon, result):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()
        gui_report_dialog(result, self.parent)

    def cleanup_and_exit(self):
        if not self.parent.get_property("visible"):
            self.disconnect_signals()
            # if the reporter selector doesn't have a parent
            if not self.parent.get_transient_for():
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
        self.disconnect_signals()
        self.assistant.destroy()

    def on_close_clicked(self, assistant, user_data=None):
        self.disconnect_signals()
        self.assistant.destroy()

    def on_apply_clicked(self, assistant, user_data=None):
        self.dehydrate()

    def on_page_prepare(self, assistant, page):
        if page == self.pdict_get_page(PAGE_CONFIRM):
            #backtrace_buff = self.backtrace_tev.get_buffer()
            #backtrace_text = backtrace_buff.get_text(backtrace_buff.get_start_iter(), backtrace_buff.get_end_iter())
            howto_buff = self.howto_tev.get_buffer()
            howto_text = howto_buff.get_text(howto_buff.get_start_iter(), howto_buff.get_end_iter())
            self.steps.set_text(howto_text)
            comment_buff = self.comment_tev.get_buffer()
            comment_text = comment_buff.get_text(comment_buff.get_start_iter(), comment_buff.get_end_iter())
            self.comments.set_text(comment_text)
        #elif page == self.pdict_get_page(PAGE_BACKTRACE_APPROVAL):
        #    self.backtrace_buff.set_text(self.report[FILENAME_BACKTRACE][CD_CONTENT])

    def on_plugin_toggled(self, togglebutton, plugins, page):
        complete = False
        for plugin in plugins:
            if plugin.get_active() is True:
                complete = True
                break
        self.assistant.set_page_complete(page, complete)

    def on_bt_toggled(self, togglebutton, page):
        self.assistant.set_page_complete(page, togglebutton.get_active())

    def pdict_add_page(self, page, name):
        # FIXME try, except??
        print "adding %s" % name
        if name not in self.pdict:
            self.pdict[name] = page
        else:
            warn("The page %s is already in the dictionary" % name)
            #raise Exception("The page %s is already in the dictionary" % name)

    def pdict_get_page(self, name):
        try:
            return self.pdict[name]
        except Exception, e:
            print e
            return None

    def prepare_page_1(self):
        plugins_cb = []
        page = gtk.VBox(spacing=10)
        page.set_border_width(10)
        lbl_default_info = gtk.Label()
        lbl_default_info.set_line_wrap(True)
        lbl_default_info.set_alignment(0.0, 0.0)
        lbl_default_info.set_justify(gtk.JUSTIFY_FILL)
        lbl_default_info.set_size_request(600, -1)
        lbl_default_info.set_markup(_("It looks like an application from the "
                                    "package <b>%s</b> has crashed "
                                    "on your system. It's a good idea to send "
                                    "a bug report about this issue. The report "
                                    "will provide software maintainers with "
                                    "information essential in figuring out how "
                                    "to provide a bug fix for you\n\n"
                                    "Please review the information that follows "
                                    "and modify it as needed to ensure your bug "
                                    "report does not contain any sensitive date "
                                    "you'd rather not share\n\n"
                                    "Select where you would like to report the "
                                    "bug, and press 'Forward' to continue.")
                                    % self.dump.getPackageName())
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
            reporters = AnalyzerActionsAndReporters[self.dump.getAnalyzerName()]
            for reporter_name in reporters.split(','):
                reporter = pluginlist.getReporterByName(reporter_name)
                if reporter:
                    self.reporters.append(reporter)
        except KeyError:
            # Analyzer has no associated reporters.
            pass
        for reporter in self.reporters:
            cb = gtk.CheckButton(str(reporter))
            cb.connect("toggled", self.on_plugin_toggled, plugins_cb, page)
            plugins_cb.append(cb)
            vbox_plugins.pack_start(cb, fill=True, expand=False)
        self.assistant.insert_page(page, PAGE_REPORTER_SELECTOR)
        self.pdict_add_page(page, PAGE_REPORTER_SELECTOR)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_INTRO)
        self.assistant.set_page_title(page, _("Send a bug report"))
        page.show_all()

    def prepare_page_2(self):
        page = gtk.VBox(spacing=10)
        page.set_border_width(10)
        lbl_default_info = gtk.Label()
        lbl_default_info.set_line_wrap(True)
        lbl_default_info.set_alignment(0.0, 0.0)
        lbl_default_info.set_justify(gtk.JUSTIFY_FILL)
        lbl_default_info.set_size_request(600, -1)
        lbl_default_info.set_text(_("Below is the backtrace associated with your "
            "crash. A crash backtrace provides developers with details about "
            "how a crash happen, helping them track down the source of the "
            "problem\n\n"
            "Please review the backtrace below and modify it as needed to "
            "ensure your bug report does not contain any sensitive date you'd "
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
        vbox_bt.pack_start(backtrace_scroll_w)
        hbox_buttons = gtk.HBox(homogeneous=True)
        button_alignment = gtk.Alignment()
        b_refresh = gtk.Button(_("Refresh"))
        b_refresh.connect("clicked", self.hydrate, 1)
        b_copy = gtk.Button(_("Copy"))
        hbox_buttons.pack_start(button_alignment)
        hbox_buttons.pack_start(b_refresh, expand=False, fill=True)
        hbox_buttons.pack_start(b_copy, expand=False, fill=True)
        vbox_bt.pack_start(hbox_buttons, expand=False, fill=False)
        backtrace_cb = gtk.CheckButton(_("I agree with submitting the backtrace"))
        backtrace_cb.connect("toggled", self.on_bt_toggled, page)
        self.assistant.insert_page(page, PAGE_BACKTRACE_APPROVAL)
        self.pdict_add_page(page, PAGE_BACKTRACE_APPROVAL)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_CONTENT)
        self.assistant.set_page_title(page, _("Approve backtrace"))
        page.pack_start(hbox_bt)
        page.pack_start(backtrace_cb, expand=False, fill=False)
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
        howto_lbl = gtk.Label(_("How this crash happen, step-by-step? "
                           "How would you reproduce it?"))
        howto_lbl.set_alignment(0.0, 0.0)
        howto_lbl.set_justify(gtk.JUSTIFY_FILL)
        self.howto_tev = gtk.TextView()
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
        comment_lbl = gtk.Label(_("Are there any comment you'd like to share "
            "with the software maintainers?"))
        comment_lbl.set_alignment(0.0, 0.0)
        comment_lbl.set_justify(gtk.JUSTIFY_FILL)
        self.comment_tev = gtk.TextView()
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
            "Please monitor what you say accordingly"))
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
            lbl.set_justify(gtk.JUSTIFY_FILL)
            table.attach(lbl, 1, 2, line, line+1,
                xoptions=gtk.FILL, yoptions=gtk.EXPAND|gtk.FILL,
                xpadding=5, ypadding=5)

            lines_in_table[table] = line+1

        page = gtk.VBox(spacing=20)
        page.set_border_width(10)
        self.assistant.insert_page(page, PAGE_CONFIRM)
        self.pdict_add_page(page, PAGE_CONFIRM)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_CONFIRM)
        self.assistant.set_page_title(page, _("Confirm and send report"))
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
        add_info_to_table(summary_table_left, _("Component"), "%s" % self.dump.get_component())
        add_info_to_table(summary_table_left, _("Package"), "%s" % self.dump.getPackageName())
        add_info_to_table(summary_table_left, _("Executable"), "%s" % self.dump.getExecutable())
        add_info_to_table(summary_table_left, _("Cmdline"), "%s" % self.dump.get_cmdline())
        #right table
        add_info_to_table(summary_table_right, _("Architecture"), "%s" % self.dump.get_arch())
        add_info_to_table(summary_table_right, _("Kernel"), "%s" % self.dump.get_kernel())
        add_info_to_table(summary_table_right, _("Release"),"%s" % self.dump.get_release())
        add_info_to_table(summary_table_right, _("Reason"), "%s" % self.dump.get_reason())

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
        backtrace_show_btn = gtk.Button(_("Click to view ..."))
        backtrace_hbox = gtk.HBox(homogeneous=True)
        hb = gtk.HBox()
        hb.pack_start(backtrace_lbl)
        hb.pack_start(backtrace_show_btn, expand=False)
        backtrace_hbox.pack_start(hb)
        alignment = gtk.Alignment()
        backtrace_hbox.pack_start(alignment)

        # steps to reporoduce
        reproduce_lbl = gtk.Label()
        reproduce_lbl.set_markup(_("<b>Steps to reporoduce:</b>"))
        reproduce_lbl.set_alignment(0.0, 0.0)
        reproduce_lbl.set_justify(gtk.JUSTIFY_LEFT)
        self.steps = gtk.Label()
        self.steps.set_alignment(0.0, 0.0)
        self.steps.set_justify(gtk.JUSTIFY_LEFT)
        #self.steps_lbl.set_text("1. Fill in information about step 1.\n"
        #                   "2. Fill in information about step 2.\n"
        #                   "3. Fill in information about step 3.\n")
        steps_aligned_hbox = gtk.HBox()
        steps_hbox = gtk.HBox(spacing=10)
        steps_hbox.pack_start(reproduce_lbl)
        steps_hbox.pack_start(self.steps)
        steps_aligned_hbox.pack_start(steps_hbox, expand=False)
        steps_aligned_hbox.pack_start(gtk.Alignment())

        # comments
        comments_lbl = gtk.Label()
        comments_lbl.set_markup(_("<b>Comments:</b>"))
        comments_lbl.set_alignment(0.0, 0.0)
        comments_lbl.set_justify(gtk.JUSTIFY_LEFT)
        self.comments = gtk.Label(_("This bug really sucks!"))
        comments_hbox = gtk.HBox(spacing=10)
        comments_hbox.pack_start(comments_lbl)
        comments_hbox.pack_start(self.comments)
        comments_aligned_hbox = gtk.HBox()
        comments_aligned_hbox.pack_start(comments_hbox, expand=False)
        comments_aligned_hbox.pack_start(gtk.Alignment())

        # pack all into the page
        page.pack_start(summary_lbl, expand=False)
        page.pack_start(basic_details_lbl, expand=False)
        page.pack_start(summary_hbox, expand=False)
        page.pack_start(backtrace_hbox, expand=False)
        page.pack_start(steps_aligned_hbox, expand=False)
        page.pack_start(comments_aligned_hbox, expand=False)
        page.show_all()

    def prepare_page_5(self):
        page = gtk.VBox(spacing=20)
        page.set_border_width(10)
        self.assistant.insert_page(page, PAGE_REPORT_DONE)
        self.pdict_add_page(page, PAGE_REPORT_DONE)
        self.assistant.set_page_type(page, gtk.ASSISTANT_PAGE_SUMMARY)
        self.assistant.set_page_title(page, _("Finish sending the bug report"))
        report_done_lbl = gtk.Label(_("Thank you for your bug report. "
            "It has been succesfully submitted. You may view your bug report "
            "online using the web adress below:"))
        report_done_lbl.set_alignment(0.0, 0.0)
        report_done_lbl.set_justify(gtk.JUSTIFY_LEFT)
        bug_reports_lbl = gtk.Label()
        bug_reports_lbl.set_alignment(0.0, 0.0)
        bug_reports_lbl.set_justify(gtk.JUSTIFY_LEFT)
        bug_reports_lbl.set_markup(_("<b>Bug reports:</b>"))
        bug_reports = gtk.Label()
        bug_reports.set_alignment(0.0, 0.0)
        bug_reports.set_justify(gtk.JUSTIFY_LEFT)
        bug_reports.set_markup(
            "<a href=\"https://bugzilla.redhat.com/show_bug.cgi?id=578425\">"
            "https://bugzilla.redhat.com/show_bug.cgi?id=578425</a>")
        bug_reports_vbox = gtk.VBox(spacing=5)
        bug_reports_vbox.pack_start(bug_reports_lbl)
        bug_reports_vbox.pack_start(bug_reports)
        page.pack_start(report_done_lbl, expand=False)
        page.pack_start(bug_reports_vbox, expand=False)
        page.show_all()

    def __del__(self):
        print "wizard: about to be deleted"

    def on_analyze_complete_cb(self, daemon, report, pBarWindow):
        try:
            gobject.source_remove(self.timer)
        except:
            pass
        self.pBarWindow.hide()
        if not report:
            gui_error_message(_("Unable to get report!\nDebuginfo is missing?"))
            return
        self.report = report
        # set the backtrace text
        self.backtrace_buff.set_text(self.report[FILENAME_BACKTRACE][CD_CONTENT])
        self.show()

    def hydrate(self, button=None, force=0):
        print "force: %i:" % force
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

        # show the report window with selected dump
        # when getReport is done it emits "analyze-complete" and on_analyze_complete_cb is called
        # FIXME: does it make sense to change it to use callback rather then signal emitting?
        try:
            self.daemon.start_job("%s:%s" % (self.dump.getUID(), self.dump.getUUID()), force)
        except Exception, ex:
            # FIXME #3  dbus.exceptions.DBusException: org.freedesktop.DBus.Error.NoReply: Did not receive a reply
            # do this async and wait for yum to end with debuginfoinstal
            if self.timer:
                gobject.source_remove(self.timer)
            self.pBarWindow.hide()
            gui_error_message(_("Error getting the report: %s" % ex))
            return

    def dehydrate(self):
        print "dehydrate"

    def show(self):
        self.assistant.show()


if __name__ == "__main__":
    wiz = ReporterAssistant()
    wiz.show()
    gtk.main()
