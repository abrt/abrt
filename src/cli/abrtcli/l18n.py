import gettext
import locale
import logging

GETTEXT_PROGNAME = "abrt"

_ = gettext.gettext
N_ = gettext.ngettext

from .config import LOCALE_DIR


def init():
    progname = GETTEXT_PROGNAME
    localedir = LOCALE_DIR

    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        locale.setlocale(locale.LC_ALL, "C")

    gettext.bindtextdomain(progname, localedir)
    gettext.textdomain(progname)
