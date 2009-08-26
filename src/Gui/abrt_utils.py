import gtk.glade
PROGNAME = "abrt"
import locale
try:
    locale.setlocale (locale.LC_ALL, "")
except locale.Error, e:
    import os
    os.environ['LC_ALL'] = 'C'
    locale.setlocale (locale.LC_ALL, "")
import gettext
gettext.bind_textdomain_codeset(PROGNAME,locale.nl_langinfo(locale.CODESET))
gettext.bindtextdomain(PROGNAME, '/usr/share/locale')
gtk.glade.bindtextdomain(PROGNAME, '/usr/share/locale')
gtk.glade.textdomain(PROGNAME)
gettext.textdomain(PROGNAME)
_ = lambda x: gettext.lgettext(x)
