AC_INIT([abrt],
	m4_esyscmd([cat ./abrt-version]),
	[crash-catcher@fedorahosted.org])

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

AC_ARG_ENABLE(gtk3,
              [AC_HELP_STRING([--enable-gtk3],
                             [Use gtk3 instead of gtk2])],
              [GTK_VER="gtk+-3.0"],[GTK_VER="gtk+-2.0"])

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

PKG_CHECK_MODULES([GTK], [$GTK_VER])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.21])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
PKG_CHECK_MODULES([RPM], [rpm])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify])
PKG_CHECK_MODULES([NSS], [nss])
PKG_CHECK_MODULES([SATYR], [satyr])
PKG_CHECK_MODULES([LIBREPORT], [libreport])
PKG_CHECK_MODULES([LIBREPORT_GTK], [libreport-gtk])
PKG_CHECK_MODULES([LIBSELINUX], [libselinux])

# Just PKG_CHECK_MODULES([PYTHON], [python]) works only with python2.7+
# Below, if python is not found, we set up for python2.6 w/o checking:
PKG_CHECK_MODULES([PYTHON], [python],,[
    PYTHON_LIBS='-L/usr/lib64 -lpython2.6'
    PYTHON_CFLAGS='-I/usr/include/python2.6'
])

AC_ARG_ENABLE([dupcheck-fingerprint],
        AS_HELP_STRING([--enable-dupcheck-fingerprint], [enable function fingerprinting for duplicate checking (x86_64 only)]),
        [], [enable_dupcheck_fingerprint=no])
AC_CANONICAL_HOST
AM_CONDITIONAL(ENABLE_DISASSEMBLY, [test "$host_cpu" = "x86_64" -a "$enable_dupcheck_fingerprint" != "no"])

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
DEFAULT_CONF_DIR='${datadir}/${PACKAGE_NAME}/conf.d'
VAR_RUN='${localstatedir}/run'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
EVENTS_DIR='${sysconfdir}/libreport/events'
DEFAULT_PLUGINS_CONF_DIR='${datadir}/${PACKAGE_NAME}/conf.d/plugins'
EVENTS_CONF_DIR='${sysconfdir}/libreport/events.d'
ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'

AC_ARG_WITH(augeaslenslibdir,
                    [AS_HELP_STRING([--with-augeaslenslibdir=DIR],
                                                   [Directory for librepor lens (default: /usr/share/augeas/lenses)])],
                                [], [with_augeaslenslibdir="/usr/share/augeas/lenses"])
AC_SUBST([AUGEAS_LENS_LIB_DIR], [$with_augeaslenslibdir])

AC_PATH_PROG(AUGPARSE, augparse, no)
[if test "$ARGPARSE" = "no"]
[then]
    [echo "The argparse program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build ABRT."]
    [exit 1]
[fi]

# Make satyr use GDB or elfutils/libunwind for unwinding?

AC_ARG_ENABLE([native-unwinder],
            AS_HELP_STRING([--enable-native-unwinder],
                        [use native unwinder (libunwind or elfutils) instead of GDB (default is no)]),
                [], [enable_native_unwinder=no])

[if test "$enable_native_unwinder" = "yes"]
[then]
    AC_DEFINE([ENABLE_NATIVE_UNWINDER], [1], [Use elfutils/libunwind for unwinding instead of GDB.])
[fi]


AC_SUBST(CONF_DIR)
AC_SUBST(DEFAULT_CONF_DIR)
AC_SUBST(VAR_RUN)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(DEFAULT_PLUGINS_CONF_DIR)
AC_SUBST(EVENTS_CONF_DIR)
AC_SUBST(EVENTS_DIR)

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//'`])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
	Makefile
	abrt.pc
	doc/Makefile
	src/include/Makefile
	src/lib/Makefile
	src/plugins/Makefile
	src/Makefile
	src/daemon/Makefile
	src/hooks/Makefile
	src/applet/Makefile
	src/gui-gtk/Makefile
	src/cli/Makefile
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
