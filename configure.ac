AC_INIT([abrt], [0.0.2], [jmoskovc@redhat.com, zprikryl@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_PROG_CXX

AM_PATH_PYTHON
if test -z "$PYTHON"; then
    echo "*** Essential program python not found" 1>&2
    exit 1
fi
    
PKG_CHECK_MODULES([SQLITE3], [sqlite3])
PKG_CHECK_MODULES([GTK], [gtk+-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([DBUSCPP], [dbus-c++-1])
PKG_CHECK_MODULES([RPM], [rpm])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify])

AC_CHECK_HEADER([sys/inotify.h], [],
   [AC_MSG_ERROR([sys/inotify.h is needed to build abrt])])

CONF_DIR='${sysconfdir}/${PACKAGE_NAME}'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
PLUGINS_LIB_DIR='${libdir}/${PACKAGE_NAME}'

DEBUG_DUMPS_DIR='${localstatedir}/cache/${PACKAGE_NAME}'
AC_ARG_WITH(debugdumpsdir, 
            [AS_HELP_STRING([--with-debugdumpdir=DIR],
                           [Directory where debugdumps are created])],
            [DEBUG_DUMPS_DIR="$withval"])

AC_SUBST(CONF_DIR)
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
	src/Makefile
	src/Daemon/Makefile
	src/Hooks/Makefile
	src/Applet/Makefile
	src/Gui/Makefile
])

AC_OUTPUT