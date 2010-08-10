bin_PROGRAMS = abrt-backtrace
abrt_backtrace_CFLAGS = -Wall
abrt_backtrace_SOURCES = abrt-backtrace.c
abrt_backtrace_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils
abrt_backtrace_LDADD = \
    ../../lib/utils/libABRTUtils.la

man_MANS = abrt-backtrace.1
EXTRA_DIST = $(man_MANS)
