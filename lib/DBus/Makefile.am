lib_LTLIBRARIES = libDBus.la
libDBus_la_SOURCES = DBusManager.cpp DBusManager.h marshal.c marshal.h
libDBus_la_LDFLAGS = -version-info 0:1:0
libDBus_la_LIBADD = $(DBUS_GLIB_LIBS)
libDBus_la_CPPFLAGS = $(DBUS_GLIB_CFLAGS)


check_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD = libDBus.la $(DL_LIBS)
test_CPPFLAGS = $(DBUS_GLIB_CFLAGS)
