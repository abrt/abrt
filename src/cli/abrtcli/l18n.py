import gettext
import locale

from .config import LOCALE_DIR

GETTEXT_PROGNAME = "abrt"

_ = gettext.gettext
N_ = gettext.ngettext


def init():
    progname = GETTEXT_PROGNAME
    localedir = LOCALE_DIR

    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        locale.setlocale(locale.LC_ALL, "C")

    gettext.bindtextdomain(progname, localedir)
    gettext.textdomain(progname)
