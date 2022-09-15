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
gl_LD_VERSION_SCRIPT
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

AC_ARG_WITH(python3,
AS_HELP_STRING([--with-python3],[build Python3 support (default is YES)]),
ABRT_PARSE_WITH([python3]))

[if test -z "$NO_PYTHON3"]
[then]
    AM_CONDITIONAL([BUILD_PYTHON3], [true])
    AM_PATH_PYTHON(
        [3.6],,
        [AC_MSG_ERROR([
    The python3 program was not found in the search path. Please ensure
    that it is installed and its directory is included in the search path or
    pass --without-python3 to ./configure.
    Then run configure again before attempting to build ABRT.
        ])]
    )

    AC_PATH_PROG([PYTHON3_CONFIG], [python${PYTHON_VERSION}-config], [no])
    [if test "$PYTHON3_CONFIG" = "no"]
    [then]
        [echo "The python${PYTHON_VERSION}-config program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-python3 to ./configure."]
        [echo "Then run configure again before attempting to build ABRT."]
        [exit 1]
    [fi]

    PYTHON3_CFLAGS=`${PYTHON3_CONFIG} --cflags 2> /dev/null`
    PYTHON3_LIBS=`${PYTHON3_CONFIG} --libs 2> /dev/null`

    AC_SUBST([PYTHON3_CFLAGS])
    AC_SUBST([PYTHON3_LIBS])
[else]
    AM_CONDITIONAL([BUILD_PYTHON3], [false])
[fi]

AC_ARG_WITH(pythondoc,
AS_HELP_STRING([--with-pythondoc],[build Python documentation (default is YES)]),
ABRT_PARSE_WITH([pythondoc]))

[if test -z "$NO_PYTHONDOC"]
[then]
    AM_CONDITIONAL(HAVE_PYTHON_SPHINX, true)
    AC_PATH_PROG([PYTHON_SPHINX], [sphinx-build-3], [no])
    [if test "$PYTHON_SPHINX" = "no"]
    [then]
        [echo "The sphinx-build-3 program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-pythondoc to ./configure."]
        [echo "Then run configure again before attempting to build ABRT."]
        [exit 1]
    [fi]
[else]
    AM_CONDITIONAL(HAVE_PYTHON_SPHINX, false)
[fi]

AC_ARG_WITH(pythontests,
AS_HELP_STRING([--with-pythontests],[run python tests (default is YES)]),
ABRT_PARSE_WITH([pythontests]))

[if test -z "$NO_PYTHONTESTS"]
[then]
    AM_CONDITIONAL(HAVE_PYTEST, true)
    AC_PATH_PROG([PYTEST], [pytest-3], [no])
    [if test "$PYTEST" = "no"]
    [then]
        [echo "The pytest-3 program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-pythontests to ./configure."]
        [echo "Then run configure again before attempting to build ABRT."]
        [exit 1]
    [fi]
[else]
    AM_CONDITIONAL(HAVE_PYTEST, false)
[fi]

GLIB_VERSION=2.55.1
LIBREPORT_VERSION=2.13.0

PKG_CHECK_MODULES([GTK], [gtk+-3.0])
PKG_CHECK_MODULES([GLIB], [gio-2.0 >= $GLIB_VERSION glib-2.0 >= $GLIB_VERSION])
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
PKG_CHECK_MODULES([LIBREPORT], [libreport >= $LIBREPORT_VERSION])
PKG_CHECK_MODULES([LIBREPORT_GTK], [libreport-gtk >= $LIBREPORT_VERSION])
PKG_CHECK_MODULES([LIBSOUP], [libsoup-3.0])
PKG_CHECK_MODULES([GIO], [gio-2.0])
PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0])
PKG_CHECK_MODULES([SATYR], [satyr])
PKG_CHECK_MODULES([SYSTEMD], [libsystemd])
PKG_CHECK_MODULES([GSETTINGS_DESKTOP_SCHEMAS], [gsettings-desktop-schemas >= 3.15.1])

PKG_PROG_PKG_CONFIG
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
VAR_RUN='${localstatedir}/run'
VAR_STATE='${localstatedir}/lib/${PACKAGE_NAME}'
PLUGINS_CONF_DIR='${sysconfdir}/${PACKAGE_NAME}/plugins'
EVENTS_DIR='${datadir}/libreport/events'
EVENTS_CONF_DIR='${sysconfdir}/libreport/events.d'
JOURNAL_CATALOG_DIR='$(prefix)/lib/systemd/catalog'
ENABLE_SOCKET_OR_DBUS='-DENABLE_DBUS=1'
DEFAULT_DUMP_LOCATION_MODE=0751
DEFAULT_DUMP_DIR_MODE=$($PKG_CONFIG --variable=dd_mode libreport)
LIBREPORT_PLUGINS_CONF_DIR=$($PKG_CONFIG --variable=plugins_conf_dir libreport)

AC_ARG_WITH([defaultdumplocation],
            AS_HELP_STRING([--with-defaultdumplocation=DIR],
                           [Default dump location ('LOCALSTATEDIR/spool/abrt')]),
            [], [with_defaultdumplocation=${localstatedir}/spool/abrt])
AC_SUBST([DEFAULT_DUMP_LOCATION], [$with_defaultdumplocation])

AC_ARG_WITH([defaultpackagemanager],
            AS_HELP_STRING([--with-defaultpackagemanager=dnf],
                           [Default package manager. 'dnf' or 'yum' ('dnf')]),
            [], [with_defaultpackagemanager=dnf])
AC_SUBST([DEFAULT_PACKAGE_MANAGER], [$with_defaultpackagemanager])


AC_ARG_WITH(augeaslenslibdir,
                    [AS_HELP_STRING([--with-augeaslenslibdir=DIR],
                                                   [Directory for librepor lens (default: /usr/share/augeas/lenses)])],
                                [], [with_augeaslenslibdir="/usr/share/augeas/lenses"])
AC_ARG_WITH([support-url],
        AS_HELP_STRING([--with-support-url=URL],
                [Specify the support URL to show in catalog entries]),
        [SUPPORT_URL="$withval"],
        [SUPPORT_URL=https://bugzilla.redhat.com/])

AC_SUBST(SUPPORT_URL)

AC_PATH_PROG(AUGPARSE, augparse, no)
[if test "$AUGPARSE" = "no"]
[then]
    [echo "The augparse program was not found in the search path. Please ensure"]
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

AC_PATH_PROG([FINDMNT], [findmnt], [/usr/bin/findmnt], [$PATH:/usr/sbin:/sbin])
AC_PATH_PROG([GDB], [gdb], [/usr/bin/gdb], [/usr/libexec$PATH_SEPARATOR$PATH])

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
AC_SUBST(VAR_RUN)
AC_SUBST(VAR_STATE)
AC_SUBST(PLUGINS_CONF_DIR)
AC_SUBST(EVENTS_CONF_DIR)
AC_SUBST(JOURNAL_CATALOG_DIR)
AC_SUBST(EVENTS_DIR)
AC_SUBST(DEFAULT_DUMP_LOCATION)
AC_SUBST(DEFAULT_DUMP_LOCATION_MODE)
AC_SUBST(DEFAULT_DUMP_DIR_MODE)
AC_SUBST(LIBREPORT_PLUGINS_CONF_DIR)

AC_ARG_WITH(bodhi,
AS_HELP_STRING([--with-bodhi],[use Bodhi plugin (default is YES)]),
ABRT_PARSE_WITH([bodhi]))

PKG_CHECK_MODULES([JSON_C], [json], [
    JSON_C_PACKAGE=json
], [
    PKG_CHECK_MODULES([JSON_C], [json-c], [
        JSON_C_PACKAGE=json-c
    ])
])


if test -z "$NO_BODHI"
then
PKG_CHECK_MODULES([LIBREPORT_WEB], [libreport-web])
AM_CONDITIONAL(BUILD_BODHI, true)
else
AM_CONDITIONAL(BUILD_BODHI, false)
fi dnl end NO_BODHI

AC_ARG_WITH(atomic,
AS_HELP_STRING([--with-atomic],[add Atomic support (default is YES)]),
ABRT_PARSE_WITH([atomic]))

if test -z "$NO_ATOMIC"
then
AM_CONDITIONAL(BUILD_ATOMIC, true)
else
AM_CONDITIONAL(BUILD_ATOMIC, false)
fi dnl end NO_BODHI

AC_ARG_WITH(selinux,
AS_HELP_STRING([--with-selinux],[build selinux support (default is YES)]),
ABRT_PARSE_WITH([selinux]))

[if test -z "$NO_SELINUX"]
[then]
    PKG_CHECK_MODULES([LIBSELINUX], [libselinux])
    AM_CONDITIONAL(HAVE_SELINUX, true)
[else]
    AM_CONDITIONAL(HAVE_SELINUX, false)
[fi]

AC_ARG_WITH(rpm,
AS_HELP_STRING([--with-rpm],[build rpm support (default is YES)]),
ABRT_PARSE_WITH([rpm]))

[if test -z "$NO_RPM"]
[then]
    PKG_CHECK_MODULES([RPM], [rpm])
    AC_DEFINE(HAVE_LIBRPM, [], [Have rpm support.])
[fi]

AC_ARG_WITH(polkit,
AS_HELP_STRING([--with-polkit],[build polkit support (default is YES)]),
ABRT_PARSE_WITH([polkit]))

[if test -z "$NO_POLKIT"]
[then]
    PKG_CHECK_MODULES([POLKIT], [polkit-gobject-1])
    PKG_CHECK_MODULES([POLKIT_AGENT], [polkit-agent-1])
    AC_DEFINE(HAVE_POLKIT, [], [Have polkit support.])
    AC_SUBST([HAVE_POLKIT], ["True"])
[else]
    AC_SUBST([HAVE_POLKIT], ["False"])
[fi]

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
	data/Makefile
	data/augeas/Makefile
	doc/Makefile
	doc/problems-service/Makefile
	src/include/Makefile
	src/lib/Makefile
	src/plugins/Makefile
	src/Makefile
	src/daemon/Makefile
	src/daemon/abrt-handle-upload
	src/hooks/Makefile
	src/applet/Makefile
	src/cli/Makefile
	src/cli/abrtcli/Makefile
	src/cli/abrtcli/cli/Makefile
	src/cli/tests/Makefile
	src/cli/tests/clitests/Makefile
	src/configuration-gui/Makefile
	src/configuration-gui/abrt_gui.pc
	src/dbus/Makefile
	src/plugins/abrt-action-analyze-vmcore
	src/plugins/abrt-action-analyze-vulnerability
	src/plugins/abrt-action-check-oops-for-hw-error
	src/plugins/abrt-action-check-oops-for-alt-component
	src/python-problem/Makefile
	src/python-problem/doc/Makefile
	src/python-problem/tests/Makefile
	src/python-problem/problem/Makefile
	src/python-problem/examples/Makefile
	po/Makefile.in
	icons/Makefile
])

AC_OUTPUT
