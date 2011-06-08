AC_INIT([libreport], [2.0.2], [crash-catcher@fedorahosted.org])

AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign silent-rules])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_LN_S

AC_SYS_LARGEFILE
CFLAGS="$CFLAGS -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE \
          -D_FILE_OFFSET_BITS=64 -fno-strict-aliasing -std=gnu99 -Wall"

AC_ARG_ENABLE(debug,
              [AC_HELP_STRING([--enable-debug],
                             [Enable debug information])],
              [CFLAGS="$CFLAGS -DDEBUG -ggdb -g"])

dnl ****** INTERNATIONALIZATION **********************
GETTEXT_PACKAGE=libreport
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

IT_PROG_INTLTOOL([0.35.0])

dnl ****** END ****************************************

AM_PATH_PYTHON
if test -z "$PYTHON"; then
    echo "*** Essential program python not found" 1>&2
    exit 1
fi

AC_PATH_PROG([ASCIIDOC], [asciidoc], [no])
[if test "$ASCIIDOC" = "no"]
[then]
    [echo "The asciidoc program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build ABRT."]
    [exit 1]
[fi]

AC_PATH_PROG([XMLTO], [xmlto], [no])
[if test "$XMLTO" = "no"]
[then]
    [echo "The xmlto program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build ABRT."]
    [exit 1]
[fi]

PKG_CHECK_MODULES([GTK], [gtk+-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.21])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
PKG_CHECK_MODULES([NSS], [nss])
PKG_CHECK_MODULES([GNOME_KEYRING], [gnome-keyring-1])
# Just PKG_CHECK_MODULES([PYTHON], [python]) works only with python2.7+
# Below, if python is not found, we set up for python2.6 w/o checking:
PKG_CHECK_MODULES([PYTHON], [python],,[
    PYTHON_LIBS='-L/usr/lib64 -lpython2.6'
    PYTHON_CFLAGS='-I/usr/include/python2.6'
])

PKG_PROG_PKG_CONFIG

# FIXME: do we want to share the /etc/abrt dir?
CONF_DIR='${sysconfdir}/abrt'
VAR_RUN='${localstatedir}/run'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
# and again share /etc/abrt ?
EVENTS_DIR='${sysconfdir}/abrt/events'
EVENTS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/events.d'
PLUGINS_LIB_DIR='${libdir}/${PACKAGE_NAME}'
LIBEXEC_DIR='${libexecdir}'

DEBUG_DUMPS_DIR='${localstatedir}/spool/${PACKAGE_NAME}'
DEBUG_INFO_DIR='${localstatedir}/cache/${PACKAGE_NAME}-di'

AC_ARG_WITH(debugdumpsdir,
            [AS_HELP_STRING([--with-debugdumpdir=DIR],
                           [Directory where debugdumps are created])],
            [DEBUG_DUMPS_DIR="$withval"])

AC_ARG_WITH(debuginfosdir,
            [AS_HELP_STRING([--with-debuginfodir=DIR],
                           [Directory where debuginfo is stored])],
            [DEBUG_INFO_DIR="$withval"])

AC_SUBST(CONF_DIR)
AC_SUBST(VAR_RUN)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(EVENTS_CONF_DIR)
AC_SUBST(EVENTS_DIR)
AC_SUBST(PLUGINS_LIB_DIR)
AC_SUBST(DEBUG_DUMPS_DIR)
AC_SUBST(DEBUG_INFO_DIR)
AC_SUBST(LIBEXEC_DIR)

# Initialize the test suite.
# AC_CONFIG_TESTDIR(tests)
# AC_CONFIG_FILES([tests/Makefile tests/atlocal])
# AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
# AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//'`])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
	Makefile
	libreport.pc
	src/include/Makefile
	src/lib/Makefile
	src/report-python/Makefile
	src/Makefile
	src/gtk-helpers/Makefile
	src/gtk-helpers/libreport-gtk.pc
	src/gui-wizard-gtk/Makefile
	po/Makefile.in
])

#	src/plugins/Makefile
#	src/cli/Makefile
#	po/Makefile.in
#	icons/Makefile
#])

AC_OUTPUT
