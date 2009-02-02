libexec_PROGRAMS = hookCCpp
hookCCpp_SOURCES = CCpp.cpp
hookCCpp_LDADD = ../../lib/DebugDump/libDebugDump.la
hookCCpp_CPPFLAGS = -I$(srcdir)/../../lib/DebugDump \
                    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"