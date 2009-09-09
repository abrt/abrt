AC_INIT([abrt], [0.0.8.5], [jmoskovc@redhat.com, zprikryl@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_ARG_ENABLE(debug,
              [AC_HELP_STRING([--enable-debug],
                             [Enable debug information])],
              [CXXFLAGS="$CXXFLAGS -DDEBUG -ggdb -g"])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CXX

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

AM_PATH_PYTHON
if test -z "$PYTHON"; then
    echo "*** Essential program python not found" 1>&2
    exit 1
fi

PKG_CHECK_MODULES([SQLITE3], [sqlite3])
PKG_CHECK_MODULES([GTK], [gtk+-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([DBUSCPP], [dbus-c++-1])
PKG_CHECK_MODULES([RPM], [rpm])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify])
PKG_CHECK_MODULES([NSS], [nss])
PKG_CHECK_MODULES([XMLRPC_CPP], [xmlrpc++])
PKG_CHECK_MODULES([XMLRPC_CLIENT_CPP], [xmlrpc_client++])
PKG_CHECK_MODULES([POLKIT],[polkit-gobject-1])

AC_CHECK_HEADER([sys/inotify.h], [],
   [AC_MSG_ERROR([sys/inotify.h is needed to build abrt])])

AC_CHECK_HEADER([magic.h], [],
   [AC_MSG_ERROR([magic.h is needed to build abrt])])
   

CONF_DIR='${sysconfdir}/${PACKAGE_NAME}'
VAR_RUN='${localstatedir}/run'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
PLUGINS_LIB_DIR='${libdir}/${PACKAGE_NAME}'
ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'

DEBUG_DUMPS_DIR='${localstatedir}/cache/${PACKAGE_NAME}'
AC_ARG_WITH(debugdumpsdir,
            [AS_HELP_STRING([--with-debugdumpdir=DIR],
                           [Directory where debugdumps are created])],
            [DEBUG_DUMPS_DIR="$withval"])

AC_ARG_ENABLE(socket,
              [AC_HELP_STRING([--enable-socket],
                             [Enable socket communication. Disable DBus communication])],
              [ENABLE_SOCKET_OR_DBUS='-DENABLE_SOCKET=1'])

AC_ARG_ENABLE(dbus,
              [AC_HELP_STRING([--enable-dbus],
                             [Enable DBus communication. Disable socket communication @<:@default@:>@])],
              [ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'])

AC_SUBST(ENABLE_SOCKET_OR_DBUS)
AC_SUBST(CONF_DIR)
AC_SUBST(VAR_RUN)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(PLUGINS_LIB_DIR)
AC_SUBST(DEBUG_DUMPS_DIR)
AC_SUBST(DL_LIBS, -ldl)

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	lib/Utils/Makefile
	lib/MiddleWare/Makefile
	lib/CommLayer/Makefile
	lib/Plugins/Makefile
	lib/Python/Makefile
	src/Makefile
	src/Daemon/Makefile
	src/Hooks/Makefile
	src/Applet/Makefile
	src/Gui/Makefile
	src/CLI/Makefile
	inc/Makefile
	po/Makefile.in
])

AC_OUTPUT
