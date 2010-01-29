import sys

PROGNAME = "abrt"
g_verbose = 0

import locale
import gettext

_ = lambda x: gettext.lgettext(x)

def init_logging(progname, v):
    import gtk.glade
    global PROGNAME, g_verbose
    PROGNAME = progname
    g_verbose = v
    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error, e:
        import os
        os.environ['LC_ALL'] = 'C'
        locale.setlocale(locale.LC_ALL, "")
    gettext.bind_textdomain_codeset(PROGNAME, locale.nl_langinfo(locale.CODESET))
    gettext.bindtextdomain(PROGNAME, '/usr/share/locale')
    gtk.glade.bindtextdomain(PROGNAME, '/usr/share/locale')
    gtk.glade.textdomain(PROGNAME)
    gettext.textdomain(PROGNAME)

def get_verbose_level():
    # Just importing g_verbose from another module doesn't work (why!?),
    # need to use a function
    return g_verbose

def log(fmt, *args):
    sys.stderr.write("%s: %s\n" % (PROGNAME, fmt % args))

def log1(fmt, *args):
    if g_verbose >= 1:
        sys.stderr.write("%s: %s\n" % (PROGNAME, fmt % args))

def log2(fmt, *args):
    if g_verbose >= 2:
        sys.stderr.write("%s: %s\n" % (PROGNAME, fmt % args))
