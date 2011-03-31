lib_LTLIBRARIES = libbtparser.la
libbtparser_la_SOURCES = \
	backtrace.h backtrace.c \
	frame.h frame.c \
	location.h location.c \
	normalize.h normalize.c \
	normalize_dbus.c \
	normalize_gdk.c \
	normalize_glib.c \
	normalize_glibc.c \
	normalize_libstdcpp.c \
	normalize_linux.c \
	normalize_xorg.c \
	thread.h thread.c \
	utils.h utils.c
libbtparser_la_CFLAGS = -Wall -Wwrite-strings -Werror -D_GNU_SOURCE -I../lib
libbtparser_la_LDFLAGS = -version-info 1:1:0
libbtparser_la_LIBADD = ../lib/libreport.la

libbtparser_includedir = $(includedir)/btparser
libbtparser_include_HEADERS = \
	backtrace.h \
	frame.h \
	location.h \
	normalize.h \
	thread.h \
	utils.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = btparser.pc
EXTRA_DIST = btparser.pc.in

# From http://www.seul.org/docs/autotut/
# Version consists 3 numbers: CURRENT, REVISION, AGE.
# CURRENT is the version of the interface the library implements.
# Whenever a new function is added, or its name changed, or
# the number or type of its parameters (the prototype -- in
# libraries we call this the function signature), this number
# goes up. And it goes up exactly by one.
#
# REVISION is the revision of the implementation of this
# interface, i.e., when you change the library by only modifying
# code inside the functions (fixing bugs, optimizing internal
# behavior, or adding/removing/changing signatures of functions
# that are private to the library -- used only internally) you
# raise the revision number only.
#
# Age is the difference between the newest and oldest interface
# the library currently implements. Let's say you had 8 versions
# of your library's interface, 0 through 7. You are now on
# the 4th revision of the 8th interface, that is, 7:3:X (remember
# we start counting on zero). And when you had to make choices
# for what old interfaces you would keep support -- for backward
# compatibility purposes, you chose to keep support for
# interfaces 5, 6 and (obviously) the current, 7. The libtool
# version of your library would be 7:3:2 , because the Age
# is 7-5 = 2.
