# copy-pasted from libreport
# ABRT_PARSE_WITH(PACKAGE)
# -----------------------
# For use in AC_ARG_WITH action-if-found, for packages default ON.
# * Set NO_PACKAGE=YesPlease for --without-PACKAGE
# * Unset NO_PACKAGE for --with-PACKAGE without ARG
AC_DEFUN([ABRT_PARSE_WITH],
    [m4_pushdef([ABRT_UC_PACKAGE], m4_toupper([$1]))dnl
    if test "$withval" = "no"; then
        NO_[]ABRT_UC_PACKAGE=YesPlease
    elif test "$withval" = "yes"; then
        NO_[]ABRT_UC_PACKAGE=
    else
        NO_[]ABRT_UC_PACKAGE=
    fi
    m4_popdef([ABRT_UC_PACKAGE])])

AC_INIT([abrt],
	m4_esyscmd([cat ./abrt-version]),
	[crash-catcher@fedorahosted.org])

AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall foreign tar-ustar])
# Support silent build rules. Disable by either passing --disable-silent-rules
# to ./configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
# The 'yes' argument is undocumented but it is common knowledge that it works.
AM_SILENT_RULES([yes])


AM_PROG_AR
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O
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

AC_PATH_PROG([PYTHON3], [python3], [no])
if test -z "$PYTHON3"; then
    echo "*** Essential program python3 not found" 1>&2
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

AC_PATH_PROG([PYTHON_CONFIG], [python-config], [no])
[if test "$PYTHON_CONFIG" = "no"]
[then]
    [echo "The python-config program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build libreport."]
    [exit 1]
[fi]

AC_PATH_PROG([PYTHON3_CONFIG], [python3-config], [no])
[if test "$PYTHON3_CONFIG" = "no"]
[then]
    [echo "The python3-config program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build libreport."]
    [exit 1]
[fi]

PYTHON_CFLAGS=`python-config --cflags 2> /dev/null`
PYTHON_LIBS=`python-config --libs 2> /dev/null`

PYTHON3_CFLAGS=`python3-config --cflags 2> /dev/null`
PYTHON3_LIBS=`python3-config --libs 2> /dev/null`

AC_SUBST([PYTHON3_PREFIX], ['${prefix}'])
AC_SUBST([PYTHON3_EXEC_PREFIX], ['${exec_prefix}'])

PYTHON3_DIR=`$PYTHON3 -c "import distutils.sysconfig; \
    print(distutils.sysconfig.get_python_lib(0,0,prefix='$PYTHON3_PREFIX'))"`
PYTHON3_EXECDIR=`$PYTHON3 -c "import distutils.sysconfig; \
    print(distutils.sysconfig.get_python_lib(1,0,prefix='$PYTHON3_EXEC_PREFIX'))"`

AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)

AC_SUBST(PYTHON3_CFLAGS)
AC_SUBST(PYTHON3_LIBS)
AC_SUBST(python3dir, $PYTHON3_DIR)
AC_SUBST(py3execdir, $PYTHON3_EXECDIR)

PKG_CHECK_MODULES([XICE], [ice])
PKG_CHECK_MODULES([XSMP], [sm])
PKG_CHECK_MODULES([GTK], [gtk+-3.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.21])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
PKG_CHECK_MODULES([RPM], [rpm])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify])
PKG_CHECK_MODULES([NSS], [nss])
PKG_CHECK_MODULES([LIBREPORT], [libreport])
PKG_CHECK_MODULES([LIBREPORT_GTK], [libreport-gtk])
PKG_CHECK_MODULES([POLKIT], [polkit-gobject-1])
PKG_CHECK_MODULES([GIO], [gio-2.0])
PKG_CHECK_MODULES([SATYR], [satyr])
PKG_CHECK_MODULES([SYSTEMD_JOURNAL], [libsystemd-journal])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir"])

AC_ARG_WITH([dbusinterfacedir],
        AS_HELP_STRING([--with-dbusinterfacedir=DIR], [Directory for dbus interface files]),
        [], [with_dbusinterfacedir=${datadir}/dbus-1/interfaces])
AC_SUBST([dbusinterfacedir], [$with_dbusinterfacedir])

AC_ARG_WITH(largedatatmpdir,
            [AS_HELP_STRING([--with-largedatatmpdir=DIR],
                           [Directory where potentially large data are created (default: /var/tmp)])],
            [], [with_largedatatmpdir="/var/tmp"])
AC_SUBST([LARGE_DATA_TMP_DIR], [$with_largedatatmpdir])

AC_CHECK_HEADER([sys/inotify.h], [],
   [AC_MSG_ERROR([sys/inotify.h is needed to build abrt])])

AC_CHECK_HEADERS([locale.h])

CONF_DIR='${sysconfdir}/${PACKAGE_NAME}'
DEFAULT_CONF_DIR='${datadir}/${PACKAGE_NAME}/conf.d'
VAR_RUN='${localstatedir}/run'
VAR_STATE='${localstatedir}/lib/${PACKAGE_NAME}'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
DEFAULT_PLUGINS_CONF_DIR='${datadir}/${PACKAGE_NAME}/conf.d/plugins'
EVENTS_DIR='${datadir}/libreport/events'
EVENTS_CONF_DIR='${sysconfdir}/libreport/events.d'
ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'
DEFAULT_DUMP_DIR_MODE=$($PKG_CONFIG --variable=dd_mode libreport)
LIBREPORT_PLUGINS_CONF_DIR=$($PKG_CONFIG --variable=plugins_conf_dir libreport)
PROBLEMS_CONFIG_INTERFACES_DIR=${dbusinterfacedir}

AC_ARG_WITH([defaultdumplocation],
            AS_HELP_STRING([--with-defaultdumplocation=DIR],
                           [Default dump location ('LOCALSTATEDIR/tmp/abrt')]),
            [], [with_defaultdumplocation=${localstatedir}/tmp/abrt])
AC_SUBST([DEFAULT_DUMP_LOCATION], [$with_defaultdumplocation])

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

AC_ARG_ENABLE(doxygen-docs,
    AS_HELP_STRING([--enable-doxygen-docs],
	[build DOXYGEN documentation (requires Doxygen)]),
    [enable_doxygen_docs=$enableval],
    [enable_doxygen_docs=no]
)

# Doxygen Documentation

AC_PATH_PROG(DOXYGEN, doxygen, no)

AC_MSG_CHECKING([whether to build Doxygen documentation])

if test "$DOXYGEN" = "no" ; then
    have_doxygen=no
else
    have_doxygen=yes
fi

if test "$enable_doxygen_docs" = "yes" ; then
    if test "$have_doxygen" = "no"; then
        AC_MSG_ERROR([Building Doxygen docs explicitly required, but Doxygen not found])
    fi
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(DBUS_DOXYGEN_DOCS_ENABLED, test "$enable_doxygen_docs" = "yes")


# Autoreporting advertisement

AC_ARG_ENABLE(suggest-autoreporting,
    AS_HELP_STRING([--enable-suggest-autoreporting],
    [print messages encouraging users to enable autoreporting]),
    [enable_suggest_autoreporting=$enableval],
    [enable_suggest_autoreporting=no]
)

AM_CONDITIONAL(SUGGEST_AUTOREPORTING, test "$enable_suggest_autoreporting" = "yes")

# Make satyr use GDB or elfutils/libunwind for unwinding?

AC_ARG_ENABLE([native-unwinder],
    AS_HELP_STRING([--enable-native-unwinder],
        [use native unwinder (libunwind or elfutils) instead of GDB (default is no)]),
    [], [enable_native_unwinder=no])

[if test "$enable_native_unwinder" = "yes"]
[then]
    AC_DEFINE([ENABLE_NATIVE_UNWINDER], [1], [Use elfutils/libunwind for unwinding instead of GDB.])
[fi]

AC_ARG_ENABLE([addon-vmcore],
    AS_HELP_STRING([--enable-addon-vmcore],
        [allow to disable vmcore addon (for architectures without kexec/kdump) (default is yes)]),
    [], [enable_addon_vmcore=yes])

[if test "$enable_addon_vmcore" = "yes"]
[then]
    AC_DEFINE([ENABLE_ADDON_VMCORE], [1], [Disable vmcore addon (for architectures without kexec/kdump).])
	AM_CONDITIONAL(BUILD_ADDON_VMCORE, true)
[else]
	AM_CONDITIONAL(BUILD_ADDON_VMCORE, false)
[fi]

# Perform stack unwind on live/dying process in the core handler?

AC_ARG_ENABLE([dump-time-unwind],
    AS_HELP_STRING([--enable-dump-time-unwind],
        [create core stack trace while the crashed process is still in memory (default is no)]),
    [], [enable_dump_time_unwind=no])

[if test "$enable_native_unwinder" = "yes" -a "$enable_dump_time_unwind" = "yes"]
[then]
    AC_DEFINE([ENABLE_DUMP_TIME_UNWIND], [1], [Create core stacktrace while the process is still in memory.])
[fi]

AC_SUBST(CONF_DIR)
AC_SUBST(DEFAULT_CONF_DIR)
AC_SUBST(VAR_RUN)
AC_SUBST(VAR_STATE)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(DEFAULT_PLUGINS_CONF_DIR)
AC_SUBST(EVENTS_CONF_DIR)
AC_SUBST(EVENTS_DIR)
AC_SUBST(DEFAULT_DUMP_LOCATION)
AC_SUBST(DEFAULT_DUMP_DIR_MODE)
AC_SUBST(LIBREPORT_PLUGINS_CONF_DIR)
AC_SUBST(PROBLEMS_CONFIG_INTERFACES_DIR)

AC_ARG_WITH(bodhi,
AS_HELP_STRING([--with-bodhi],[use Bodhi plugin (default is YES)]),
ABRT_PARSE_WITH([bodhi]))

if test -z "$NO_BODHI"
then
PKG_CHECK_MODULES([JSON_C], [json], [
    JSON_C_PACKAGE=json
], [
    PKG_CHECK_MODULES([JSON_C], [json-c], [
        JSON_C_PACKAGE=json-c
    ])
])
PKG_CHECK_MODULES([LIBREPORT_WEB], [libreport-web])
AM_CONDITIONAL(BUILD_BODHI, true)
else
AM_CONDITIONAL(BUILD_BODHI, false)
fi dnl end NO_BODHI

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
# CFLAGS may contain '-Werror=format-security'
AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//' | sed 's/-Werror\(=[[^ ]]*\)\?//g'`])

AC_CONFIG_HEADERS([config.h])

AC_CHECK_PROGS([VALGRIND], [valgrind])

AC_CONFIG_FILES([
	Makefile
	abrt.pc
	apidoc/Makefile
	apidoc/Doxyfile
	apidoc/html/Makefile
	doc/Makefile
	doc/problems-service/Makefile
	doc/dbus-configuration/Makefile
	src/include/Makefile
	src/lib/Makefile
	src/plugins/Makefile
	src/Makefile
	src/daemon/Makefile
	src/daemon/abrt-handle-upload
	src/hooks/Makefile
	src/applet/Makefile
	src/cli/Makefile
	src/configuration-gui/Makefile
	src/configuration-gui/abrt_gui.pc
	src/dbus/Makefile
	src/plugins/abrt-action-install-debuginfo
	src/plugins/abrt-action-analyze-vmcore
	src/plugins/abrt-action-check-oops-for-hw-error
	src/python-problem/Makefile
	src/python-problem/doc/Makefile
	src/python-problem/tests/Makefile
	src/python-problem/problem/Makefile
	src/python-problem/examples/Makefile
	po/Makefile.in
	icons/Makefile
])

AC_OUTPUT
