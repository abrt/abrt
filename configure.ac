AC_INIT([abrt],
	m4_esyscmd([cat ./abrt-version]),
	[crash-catcher@fedorahosted.org])

AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall foreign])
# Support silent build rules. Disable by either passing --disable-silent-rules
# to ./configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
# The 'yes' argument is undocumented but it is common knowledge that it works.
AM_SILENT_RULES([yes])


AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_LN_S

AC_SYS_LARGEFILE
CFLAGS="$CFLAGS -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE \
          -D_FILE_OFFSET_BITS=64 -fno-strict-aliasing -std=gnu99 -Wall -Wwrite-strings"

AC_ARG_ENABLE(debug,
              [AC_HELP_STRING([--enable-debug],
                             [Enable debug information])],
              [CFLAGS="$CFLAGS -DDEBUG -ggdb -g -O0"])

dnl ****** INTERNATIONALIZATION **********************
GETTEXT_PACKAGE=abrt
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

AC_PATH_PROG([PYTHON], [python-config], [no])
[if test "$XMLTO" = "no"]
[then]
    [echo "The python-config program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build libreport."]
    [exit 1]
[fi]

PYTHON_CFLAGS=`python-config --cflags 2> /dev/null`
PYTHON_LIBS=`python-config --libs 2> /dev/null`
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)

PKG_CHECK_MODULES([GTK], [gtk+-3.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.21])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
PKG_CHECK_MODULES([RPM], [rpm])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify])
PKG_CHECK_MODULES([NSS], [nss])
PKG_CHECK_MODULES([BTPARSER], [btparser])
PKG_CHECK_MODULES([LIBREPORT], [libreport])
PKG_CHECK_MODULES([LIBREPORT_GTK], [libreport-gtk])
PKG_CHECK_MODULES([LIBREPORT_WEB], [libreport-web])
PKG_CHECK_MODULES([POLKIT], [polkit-gobject-1])
PKG_CHECK_MODULES([GIO], [gio-2.0])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir"])

AC_CHECK_HEADER([sys/inotify.h], [],
   [AC_MSG_ERROR([sys/inotify.h is needed to build abrt])])

AC_CHECK_HEADERS([locale.h])

CONF_DIR='${sysconfdir}/${PACKAGE_NAME}'
VAR_RUN='${localstatedir}/run'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
EVENTS_DIR='${sysconfdir}/libreport/events'
EVENTS_CONF_DIR='${sysconfdir}/libreport/events.d'
ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'
DEFAULT_DUMP_LOCATION='${localstatedir}/spool/abrt'
DEFAULT_DUMP_DIR_MODE=$($PKG_CONFIG --variable=dd_mode libreport)

AC_SUBST(CONF_DIR)
AC_SUBST(VAR_RUN)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(EVENTS_CONF_DIR)
AC_SUBST(EVENTS_DIR)
AC_SUBST(DEFAULT_DUMP_LOCATION)
AC_SUBST(DEFAULT_DUMP_DIR_MODE)

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//' | sed 's/-Werror//'`])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
	Makefile
	abrt.pc
	doc/Makefile
	doc/problems-service/Makefile
	src/include/Makefile
	src/lib/Makefile
	src/plugins/Makefile
	src/Makefile
	src/daemon/Makefile
	src/hooks/Makefile
	src/applet/Makefile
	src/gui-gtk/Makefile
	src/cli/Makefile
	src/dbus/Makefile
	src/plugins/abrt-action-install-debuginfo
	src/plugins/abrt-action-analyze-vmcore
	src/python-problem/Makefile
	src/python-problem/doc/Makefile
	src/python-problem/tests/Makefile
	src/python-problem/problem/Makefile
	src/python-problem/examples/Makefile
	po/Makefile.in
	icons/Makefile
])

AC_OUTPUT
