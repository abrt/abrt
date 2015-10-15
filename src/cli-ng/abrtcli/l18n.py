import gettext
import locale
import humanize
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

    try:
        humanize.i18n.activate(lcl)
    except IOError as ex:
        logging.debug("Unsupported locale '{0}': {1}".format(lcl, str(ex)))

    gettext.bind_textdomain_codeset(progname,
                                    locale.nl_langinfo(locale.CODESET))
    gettext.bindtextdomain(progname, localedir)
    gettext.textdomain(progname)
