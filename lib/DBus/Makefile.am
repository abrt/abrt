lib_LTLIBRARIES = libDBus.la
libDBus_la_SOURCES = DBusManager.cpp DBusManager.h DBusClient.h DBusClient.cpp \
                    DBusClientProxy.h DBusServer.cpp DBusServer.h DBusServerProxy.h
libDBus_la_LDFLAGS = -version-info 0:1:0
libDBus_la_LIBADD = $(DBUS_GLIB_LIBS) $(GLIBMM_LIBS) $(DBUSCPP_LIBS)
libDBus_la_CPPFLAGS = $(DBUS_GLIB_CFLAGS) $(GLIBMM_CFLAGS) $(DBUSCPP_CFLAGS) \
                        -I../../lib/MiddleWare

#BUILT_SOURCES = crashcatcher-glue.h

#crashcatcher-glue.h: crashcatcher.xml
#	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=Crash --mode=glib-server --output=crashcatcher-glue.h $(srcdir)/crashcatcher.xml

check_PROGRAMS = test_server test_client
test_server_SOURCES = test_server.cpp
test_server_LDADD = libDBus.la $(DL_LIBS)
test_server_CPPFLAGS = $(DBUS_GLIB_CFLAGS) $(DBUSCPP_CFLAGS) -g -I../../lib/MiddleWare
test_client_SOURCES = test_client.cpp
test_client_LDADD = libDBus.la $(DL_LIBS)
test_client_CPPFLAGS = $(DBUS_GLIB_CFLAGS) $(DBUSCPP_CFLAGS) -g -I../../lib/MiddleWare
