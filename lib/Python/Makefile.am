pyexec_LTLIBRARIES = ABRTUtils.la
ABRTUtils_la_SOURCES = PyDebugDump.cpp
ABRTUtils_la_LDFLAGS = -module -avoid-version
ABRTUtils_la_LIBADD = ../Utils/libABRTUtils.la
ABRTUtils_la_CPPFLAGS = -fPIC -shared `python-config --cflags` -I../Utils
