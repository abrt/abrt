# -*- coding: utf-8 -*-
from glib import markup_escape_text
import gtk
import pango
import subprocess
import sys

try:
    # we don't want to add dependency to rpm, but if we have it, we can use it
    import rpm
except ImportError:
    rpm = None
from abrt_utils import _, log, log1, log2


def tag_urls_in_text(text):
    url_marks = ["http://", "https://", "ftp://", "ftps://", "file://"]
    text = markup_escape_text(text)
    lines = text.split('\n')
    lines_dict = {}
    for index in xrange(len(lines)):
        lines_dict[index] = lines[index]

    for mark in url_marks:
        for ix,line in lines_dict.items():
            last_mark = line.find(mark)
            if last_mark != -1:
                url_end = line.find(' ',last_mark)
                if url_end == -1:
                    url_end = len(line)
                url = line[last_mark:url_end]
                tagged_url = "<a href=\"%s\">%s</a>" % (url, url)
                lines_dict[ix] = line.replace(url, tagged_url)
    retval = ""
    for line in lines_dict.itervalues():
        retval += line
        retval +='\n'
    # strip the trailing \n
    return retval[:-1]

def on_label_resize(label, allocation):
    label.set_size_request(allocation.width,-1)

def gui_report_dialog ( report_status_dict, parent_dialog,
                      message_type=gtk.MESSAGE_INFO,
                      widget=None, page=0, broken_widget=None ):
    MAX_WIDTH = 50
    builder = gtk.Builder()
    builderfile = "%s%sdialogs.glade" % (sys.path[0],"/")
    builder.add_from_file(builderfile)
    dialog = builder.get_object("ReportDialog")
    dialog.set_geometry_hints(dialog, min_width=450, min_height=150)
    dialog.set_resizable(True)
    main_hbox = builder.get_object("main_hbox")

    STATUS = 0
    MESSAGE = 1
    status_vbox = gtk.VBox()
    for plugin, res in report_status_dict.iteritems():
        plugin_status_vbox = gtk.VBox()
        plugin_label = gtk.Label()
        plugin_label.set_markup("<b>%s</b>: " % plugin)
        plugin_label.set_justify(gtk.JUSTIFY_RIGHT)
        plugin_label.set_alignment(0, 0)
        status_label = gtk.Label()
        status_label.connect("size-allocate",on_label_resize)
        status_label.set_max_width_chars(MAX_WIDTH)
        status_label.set_size_request(400,-1)
        status_label.set_selectable(True)
        status_label.set_line_wrap(True)
        status_label.set_line_wrap_mode(pango.WRAP_CHAR)
        status_label.set_alignment(0, 0)
        plugin_status_vbox.pack_start(plugin_label, expand=False)
        plugin_status_vbox.pack_start(status_label, fill=True, expand=True)
        # 0 means not succesfull
        #if report_status_dict[plugin][STATUS] == '0':
        # this first one is actually a fallback to set at least
        # a raw text in case when set_markup() fails
        status_label.set_text(report_status_dict[plugin][MESSAGE])
        status_label.set_markup("<span foreground='red'>%s</span>" % markup_escape_text(report_status_dict[plugin][MESSAGE]))
        # if the report was not succesful then this won't pass so this runs only
        # if report succeds and gets overwriten by the status message
        if report_status_dict[plugin][STATUS] == '1':
            status_label.set_markup(tag_urls_in_text(report_status_dict[plugin][MESSAGE]))

        if len(report_status_dict[plugin][1]) > MAX_WIDTH:
            status_label.set_tooltip_text(report_status_dict[plugin][1])
        status_vbox.pack_start(plugin_status_vbox, fill=True, expand=False)
    main_hbox.pack_start(status_vbox)

    if widget != None:
        if isinstance (widget, gtk.CList):
            widget.select_row (page, 0)
        elif isinstance (widget, gtk.Notebook):
            widget.set_current_page (page)
    if broken_widget != None:
        broken_widget.grab_focus ()
        if isinstance (broken_widget, gtk.Entry):
            broken_widget.select_region (0, -1)

    if parent_dialog:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent_dialog)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    main_hbox.show_all()
    ret = dialog.run()
    dialog.destroy()
    return ret

def gui_info_dialog ( message, parent=None,
                      message_type=gtk.MESSAGE_INFO,
                      widget=None, page=0, broken_widget=None ):

    dialog = gtk.MessageDialog( parent,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_OK,
                               message )
    dialog.set_markup(message)
    if widget != None:
        if isinstance (widget, gtk.CList):
            widget.select_row (page, 0)
        elif isinstance (widget, gtk.Notebook):
            widget.set_current_page (page)
    if broken_widget != None:
        broken_widget.grab_focus ()
        if isinstance (broken_widget, gtk.Entry):
            broken_widget.select_region (0, -1)

    if parent:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    ret = dialog.run ()
    dialog.destroy()
    return ret

def gui_error_message ( message, parent_dialog=None,
                      message_type=gtk.MESSAGE_ERROR,
                      widget=None, page=0, broken_widget=None ):

    dialog = gtk.MessageDialog( parent_dialog,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_OK, message )
    dialog.set_markup(message)
    if parent_dialog:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent_dialog)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    ret = dialog.run ()
    dialog.destroy()
    return ret

def gui_question_dialog ( message, parent_dialog=None,
                      message_type=gtk.MESSAGE_QUESTION,
                      widget=None, page=0, broken_widget=None ):

    dialog = gtk.MessageDialog( parent_dialog,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_NONE,
                               message )
    dialog.add_button("gtk-cancel", gtk.RESPONSE_CANCEL)
    dialog.add_button("gtk-no", gtk.RESPONSE_NO)
    dialog.add_button("gtk-yes", gtk.RESPONSE_YES)
    dialog.set_markup(message)
    if parent_dialog:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent_dialog)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    ret = dialog.run ()
    dialog.destroy()
    return ret

def get_icon_for_package(theme, package):
    log2("get_icon_for_package('%s')", package)
    package_icon = None
    try:
        package_icon = theme.load_icon(package, 48, gtk.ICON_LOOKUP_USE_BUILTIN)
    except:
        # try to find icon filename by manually
        if not rpm:
            return None
        ts = rpm.TransactionSet()
        mi = ts.dbMatch('name', package)
        possible_icons = []
        icon_filename = ""
        icon_name = ""
        filenames = ""
        for h in mi:
            filenames = h['filenames']
        for filename in filenames:
            # add check only for last 4 chars
            if filename.rfind(".png") != -1:
                possible_icons.append(filename)
            if filename.rfind(".desktop") != -1:
                log2("desktop file:'%s'", filename)
                desktop_file = open(filename, 'r')
                lines = desktop_file.readlines()
                for line in lines:
                    if line.find("Icon=") != -1:
                        log2("Icon='%s'", line[5:-1])
                        icon_name = line[5:-1]
                        break
                desktop_file.close()
        # .desktop file found
        if icon_name:
            try:
                package_icon = theme.load_icon(icon_name, 48, gtk.ICON_LOOKUP_USE_BUILTIN)
            except:
                # we should get here only if the .desktop file is wrong..
                for filename in h['filenames']:
                    if filename.rfind("%s.png" % icon_name) != -1:
                        icon_filename = filename
                        # if we found size 48x48 we don't need to continue
                        if "48x48" in icon_filename:
                            log2("png file:'%s'", filename)
                            break
        if icon_filename:
            log1("icon created from %s", icon_filename)
            package_icon = gtk.gdk.pixbuf_new_from_file_at_size(icon_filename, 48, 48)
    return package_icon

def show_log(message_log, parent=None):
    builder = gtk.Builder()
    builderfile = "%s%sdialogs.glade" % (sys.path[0],"/")
    builder.add_from_file(builderfile)
    dialog = builder.get_object("LogViewer")
    tevLog = builder.get_object("tevLog")

    if parent:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    buff = gtk.TextBuffer()
    buff.set_text(message_log)
    tevLog.set_buffer(buff)

    dialog.run()
    dialog.destroy()

if __name__ == "__main__":
    window = gtk.Window()
    gui_report_dialog("<b>Bugzilla</b>: <span foreground='red'>CReporterBugzilla::Report(): CReporterBugzilla::Login(): RPC response indicates failure.  The username or password you entered is not valid.</span>\n<b>Logger</b>: Report was stored into: /var/log/abrt.log", window)
    gtk.main()
