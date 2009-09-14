#bin_PROGRAMS = abrt-cli
#abrt_cli_SOURCES = ABRTSocket.cpp ABRTSocket.h CLI.cpp
#abrt_cli_CPPFLAGS = -I$(srcdir)/../../inc -DVAR_RUN=\"$(VAR_RUN)\"

bin_PROGRAMS = abrt-cli
abrt_cli_SOURCES = \
    CLI.cpp \
    ABRTSocket.h ABRTSocket.cpp \
    CommLayerClientDBus.h CommLayerClientDBus.cpp \
abrt_cli_CPPFLAGS = \
    $(DBUS_GLIB_CFLAGS) $(GTK_CFLAGS) $(DBUSCPP_CFLAGS) $(LIBNOTIFY_CFLAGS)\
    -I$(srcdir)/../../inc -DVAR_RUN=\"$(VAR_RUN)\" \
    -I$(srcdir)/../../lib/CommLayer \
    $(ENABLE_SOCKET_OR_DBUS) \
    -I../../lib/MiddleWare

abrt_cli_LDADD = ../../lib/CommLayer/libABRTCommLayer.la
