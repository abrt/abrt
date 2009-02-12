AC_INIT([CrashCatcher], [0.0.1], [jmoskovc@redhat.com, zprikryl@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_PROG_CXX

PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
PKG_CHECK_MODULES([SQLITE3], [sqlite3])
PKG_CHECK_MODULES([GTKMM], [gtkmm-2.4])
PKG_CHECK_MODULES([PACKAGEKIT_GLIB], [packagekit-glib])
PKG_CHECK_MODULES([RPM], [rpm])

AC_CHECK_HEADER([sys/inotify.h], [],
   [AC_MSG_ERROR([sys/inotify.h is needed to build CrashCatcher])])

CONF_DIR='${sysconfdir}/${PACKAGE_NAME}'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/Plugins'
PLUGINS_LIB_DIR='${libdir}/${PACKAGE_NAME}'

DEBUG_DUMPS_DIR='${prefix}/var/cache/${PACKAGE_NAME}'
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
	lib/DBus/Makefile
	lib/Utils/Makefile
	lib/MiddleWare/Makefile
	lib/Plugins/Makefile
	src/Makefile
	src/Daemon/Makefile
	src/Hooks/Makefile
	src/Applet/Makefile
])

AC_OUTPUT