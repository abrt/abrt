# -*- coding: utf-8 -*-
## Copyright (C) 2001-2005 Red Hat, Inc.
## Copyright (C) 2001-2005 Harald Hoyer <harald@redhat.com>

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

"""
Module for a userfriendly exception handling

Example code:

import sys

from exception import action, error, exitcode, installExceptionHandler

installExceptionHandler("test", "1.0", gui=0, debug=0)

def exception_function():
    action("Trying to divide by zero")

    try:
        local_var_1 = 1
        local_var_2 = 0
        # test exception raised to show the effect
        local_var_3 = local_var_1 / local_var_2
    except:
        error("Does not seem to work!? :-)")
        exitcode(15)
        raise

"""
import sys

from rhpl.translate import _


__DUMPHASH = {}
# FIXME: do length limits on obj dumps.
def __dump_class(instance, fd, level=0):
    "dumps all classes"
    import types
    # protect from loops
    if not __DUMPHASH.has_key(instance):
        __DUMPHASH[instance] = True
    else:
        fd.write("Already dumped\n")
        return
    if (instance.__class__.__dict__.has_key("__str__") or
        instance.__class__.__dict__.has_key("__repr__")):
        fd.write("%s\n" % (instance,))
        return
    fd.write("%s instance, containing members:\n" %
             (instance.__class__.__name__))
    pad = ' ' * ((level) * 2)
    for key, value in instance.__dict__.items():
        if type(value) == types.ListType:
            fd.write("%s%s: [" % (pad, key))
            first = 1
            for item in value:
                if not first:
                    fd.write(", ")
                else:
                    first = 0
                if type(item) == types.InstanceType:
                    __dump_class(item, fd, level + 1)
                else:
                    fd.write("%s" % (item,))
            fd.write("]\n")
        elif type(value) == types.DictType:
            fd.write("%s%s: {" % (pad, key))
            first = 1
            for k, v in value.items():
                if not first:
                    fd.write(", ")
                else:
                    first = 0
                if type(k) == types.StringType:
                    fd.write("'%s': " % (k,))
                else:
                    fd.write("%s: " % (k,))
                if type(v) == types.InstanceType:
                    __dump_class(v, fd, level + 1)
                else:
                    fd.write("%s" % (v,))
            fd.write("}\n")
        elif type(value) == types.InstanceType:
            fd.write("%s%s: " % (pad, key))
            __dump_class(value, fd, level + 1)
        else:
            fd.write("%s%s: %s\n" % (pad, key, value))

def __dump_exception(out, text, tracebk):
    'write a traceback to "out"'
    #from cPickle import Pickler
    #p = Pickler(out)

    out.write(text)

    trace = tracebk
    while trace.tb_next:
        trace = trace.tb_next
    frame = trace.tb_frame
    out.write ("\nLocal variables in innermost frame:\n")
    try:
        for (key, value) in frame.f_locals.items():
            out.write ("%s: %s\n" % (key, value))
    except:
        pass


def __exception_window(title, text, component_name):
    "Creates a dialog and displays the exception"
    import gtk
    win = gtk.Dialog(title, None, gtk.DIALOG_MODAL)
    win.add_button(_("_Debug"), 1)
    win.add_button(_("_Save to file"), 2)
    win.add_button(gtk.STOCK_QUIT, 0)
    win.set_border_width(6)
    mbuffer = gtk.TextBuffer(None)
    mbuffer.set_text(text)
    textbox = gtk.TextView()
    textbox.set_buffer(mbuffer)
    textbox.set_property("editable", False)
    textbox.set_property("cursor_visible", False)
    scw = gtk.ScrolledWindow ()
    scw.set_shadow_type(gtk.SHADOW_IN)
    scw.add (textbox)
    scw.set_policy (gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
    hbox = gtk.HBox (False)
    hbox.set_border_width(6)
    txt = _("An unhandled exception has occurred.  This "
            "is most likely a bug.  Please save the crash "
            "dump and file a detailed bug "
            "report against %s at "
            "https://bugzilla.redhat.com/bugzilla") % \
            component_name
    info = gtk.Label(txt)
    info.set_line_wrap(True)
    hbox.pack_start (scw, True)
    # pylint: disable-msg=E1101
    win.vbox.pack_start (info, False) 
    win.vbox.pack_start (hbox, True)  
    win.vbox.set_border_width(12)     
    win.vbox.set_spacing(12)          
    win.set_size_request (500, 300)
    win.set_position (gtk.WIN_POS_CENTER)
    #contents = win.get_children()[0]
    win.show_all ()
    rc = win.run ()
    win.destroy()
    return rc


def _generic_error_dialog (title, message, parent_dialog,
                            message_type=None,
                            widget=None, page=0, broken_widget=None):
    import gtk

    if message_type == None:
        message_type = gtk.MESSAGE_ERROR

    dialog = gtk.MessageDialog(parent_dialog,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_OK,
                               message)
    dialog.set_title(title)

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

    ret = dialog.run ()
    dialog.destroy()
    return ret

__ACTION_STR = ""
def action(what):
    """Describe what you want to do actually.
    what - string
    """
    global __ACTION_STR # pylint: disable-msg=W0603
    __ACTION_STR = what

__ERROR_STR = ""
def error(what):
    """Describe what went wrong with a userfriendly text.
    what - string
    """
    global __ERROR_STR # pylint: disable-msg=W0603
    __ERROR_STR = what

__EXITCODE = 10
def exitcode(num):
    """The exitcode, with which the exception handling routine should call
    sys.exit().
    num - int(exitcode)
    """
    global __EXITCODE # pylint: disable-msg=W0603
    __EXITCODE = int(num)

#
# handleMyException function
#
def handleMyException((etype, value, tb), progname, version, 
                      gui = 1, debug = 1):
    """
    The exception handling function.

    progname - the name of the application
    version  - the version of the application
    gui      - display a gtk dialog (0, 1) to show the error message
    debug    - show the full traceback (with "Save to file" in GUI)
    """
    if not debug:
        if not gui:
            print _("Error: %s: %s") % (__ACTION_STR, __ERROR_STR)
        else:
            import gtk
            text = _("%s\n\n%s:\n%s") % (progname, __ACTION_STR, __ERROR_STR)
            _generic_error_dialog(progname, text, None)

        sys.exit(__EXITCODE)

    # restore original exception handler
    sys.excepthook = sys.__excepthook__  # pylint: disable-msg=E1101

    import os.path
    import md5
    import traceback

    elist = traceback.format_exception (etype, value, tb)
    tblast = traceback.extract_tb(tb, limit=None)
    if len(tblast):
        tblast = tblast[len(tblast)-1]
    extxt = traceback.format_exception_only(etype, value)
    if progname:
        text = "Component: %s\n" % progname
    if version:
        text = text + "Version: %s\n" % version
    text = text + "Summary: TB"
    if tblast and len(tblast) > 3:
        ll = []
        ll.extend(tblast[:3])
        ll[0] = os.path.basename(tblast[0])
        tblast = ll

    m = md5.new()
    ntext = ""
    for t in tblast:
        ntext += str(t) + ":"
        m.update(str(t))


    text += str(m.hexdigest())[:8] + " " + ntext

    text += extxt[0]
    text += "\n"
    text += "".join(elist)

    trace = tb
    while trace.tb_next:
        trace = trace.tb_next
    frame = trace.tb_frame
    text += ("\nLocal variables in innermost frame:\n")
    try:
        for (key, value) in frame.f_locals.items():
            text += "%s: %s\n" % (key, value)
    except:
        pass

    if not gui:
        print text
        sys.exit(__EXITCODE)

    import gtk # pylint: disable-msg=W0404
    if not debug:
        _generic_error_dialog(progname, text, None)
        sys.exit(__EXITCODE)

    while 1:
        rc = __exception_window (_("%(progname)s - Exception Occurred") \
                                    % {'progname' : progname}, 
                                text, progname)
        print text

        if rc == 1 and tb:
            import pdb
            import signal
            pdb.post_mortem (tb)
            os.kill(os.getpid(), signal.SIGKILL)
        elif not rc:
            sys.exit(__EXITCODE)
        else:
            d = gtk.FileChooserDialog(_("Specify a file to save the dump"),
                                      None, gtk.FILE_CHOOSER_ACTION_SAVE,
                                      (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                                       gtk.STOCK_SAVE, gtk.RESPONSE_OK))
            d.set_default_response(gtk.RESPONSE_OK)
            rc = d.run()
            if rc == gtk.RESPONSE_OK:
                tfile = d.get_filename()
                d.destroy()

                if not tfile:
                    tfile = "/tmp/dump"

                try:
                    out = open(tfile, "w")
                    out.write(text)
                    out.close()

                except IOError:
                    _generic_error_dialog(progname, 
                                           _("Failed to write to file %s.") \
                                               % (tfile), None)
                else:
                    _generic_error_dialog(progname,
                        _("The application's state has been successfully\n"
                          "written to the file '%s'.") % (tfile), None,
                        message_type = "info")
                    sys.exit(__EXITCODE)
            else:
                d.destroy()
                continue

    sys.exit(__EXITCODE)

def installExceptionHandler(progname, version, gui = 1, debug = 1):
    """
    Install the exception handling function.

    progname - the name of the application
    version  - the version of the application
    gui      - display a gtk dialog (0, 1) to show the error message
    debug    - show the full traceback (with "Save to file" in GUI)
    """
    sys.excepthook = lambda etype, value, tb: \
        handleMyException((etype, value, tb), 
                          progname, version, gui, debug)

if __name__ == '__main__':
    def _exception_function():
        action("Trying to divide by zero")

        try:
            local_var_1 = 1
            local_var_2 = 0
            # test exception raised to show the effect
            local_var_3 = local_var_1 / local_var_2 # pylint: disable-msg=W0612
        except:
            error("Does not seem to work!? :-)")
            exitcode(15)
            raise

    def _usage():
        print """%s [-dgh] [--debug] [--gui] [--help]
    -d, --debug
        Show the whole backtrace

    -g, --gui
        Display a gtk error dialog

    -h, --help
        Display this message""" % (sys.argv[0])

    import getopt
    __debug = 1
    __gui = 0

    installExceptionHandler("test", "1.0", __gui, __debug)

    __debug = 0

    class BadUsage(Exception):
        "exception for a bad command line usage"

    try:
        __opts, __args = getopt.getopt(sys.argv[1:], "dgh",
                                   [
                                    "debug",
                                    "help",
                                    "gui",
                                    ])

        for __opt, __val in __opts:
            if __opt == '-d' or __opt == '--debug':
                __debug = 1
                continue

            if __opt == '-g' or __opt == '--gui':
                __gui = 1
                continue

            if __opt == '-h' or __opt == '--help':
                _usage()
                sys.exit(0)

    except (getopt.error, BadUsage):
        _usage()
        sys.exit(1)

    installExceptionHandler("test", "1.0", __gui, __debug)

    _exception_function()
    sys.exit(0)


__author__ = "Harald Hoyer <harald@redhat.com>"
