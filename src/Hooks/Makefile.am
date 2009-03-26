libexec_PROGRAMS = hookCCpp

# CCpp
hookCCpp_SOURCES = CCpp.cpp
hookCCpp_LDADD = ../../lib/Utils/libUtils.la
hookCCpp_CPPFLAGS = -I$(srcdir)/../../lib/Utils \
                    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"
