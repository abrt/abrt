libexec_PROGRAMS = hookCCpp hookKerneloops

# CCpp
hookCCpp_SOURCES = CCpp.cpp
hookCCpp_LDADD = ../../lib/Utils/libUtils.la
hookCCpp_CPPFLAGS = -I$(srcdir)/../../lib/Utils \
                    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"

# Kerneloops
hookKerneloops_SOURCES = Kerneloops.cpp ../../lib/Plugins/KerneloopsDmesg.cpp ../../lib/Plugins/KerneloopsDmesg.h
hookKerneloops_LDADD = ../../lib/Utils/libUtils.la
hookKerneloops_CPPFLAGS = -static \
                          -I$(srcdir)/../../lib/Utils \
                          -I$(srcdir)/../../lib/Plugins \
                          -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"
