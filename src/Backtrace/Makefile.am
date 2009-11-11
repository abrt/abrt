bin_PROGRAMS = abrt-backtrace

#BUILT_SOURCES = parser.h
#AM_YFLAGS = -d
AM_YFLAGS = --verbose

abrt_backtrace_SOURCES = \
    main.c \
    backtrace.h backtrace.c \
    strbuf.h strbuf.c \
    fallback.h fallback.c \
    parser.y

#abrt_backtrace_CPPFLAGS = \
#    -I$(srcdir)/../../inc \
#    -I$(srcdir)/../../lib/Utils

#abrt_backtrace_LDADD = \
#    ../../lib/Utils/libABRTUtils.la

#man_MANS = abrt-backtrace.1
#EXTRA_DIST = $(man_MANS)
