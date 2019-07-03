import gettext
import locale
import logging

GETTEXT_PROGNAME = "abrt"

_ = gettext.gettext
ngettext = gettext.ngettext

from .config import LOCALE_DIR


def init():
    progname = GETTEXT_PROGNAME
    localedir = LOCALE_DIR

    lcl = 'C'
    try:
        lcl = locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        import os
        os.environ['LC_ALL'] = 'C'
        lcl = locale.setlocale(locale.LC_ALL, "")

    gettext.bindtextdomain(progname, localedir)
    gettext.textdomain(progname)
