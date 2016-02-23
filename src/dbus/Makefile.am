sbin_PROGRAMS = \
    abrt-dbus \
    abrt-configuration

noinst_PROGRAMS = \
    abrt-problems2

noinst_LIBRARIES = \
    libabrt-problems2-service.a

abrt_dbus_SOURCES = \
    abrt-dbus.c \
    abrt-polkit.c \
    abrt-polkit.h
abrt_dbus_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GIO_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
abrt_dbus_LDADD = \
    libabrt-problems2-service.a \
    $(GIO_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(POLKIT_LIBS) \
    -labrt_dbus

libabrt_problems2_service_a_SOURCES = \
    abrt_problems2_service.c \
    abrt_problems2_service.h \
    abrt_problems2_session.c \
    abrt_problems2_session.h \
    abrt_problems2_entry.c \
    abrt_problems2_entry.h \
    abrt_problems2_task.c \
    abrt_problems2_task.h \
    abrt_problems2_task_new_problem.c \
    abrt_problems2_task_new_problem.h
# Manual dependency
libabrt_problems2_service_a-abrt_problems2_service.$(OBJEXT): \
    abrt_problems2_generated_interfaces.h
libabrt_problems2_service_a_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GIO_CFLAGS) \
    $(GIO_UNIX_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE

abrt_problems2_SOURCES = \
    abrt_problems2.c

abrt_problems2_CPPFLAGS = \
    -I$(srcdir)/../include \
    $(GIO_CFLAGS) \
    $(GIO_UNIX_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
abrt_problems2_LDADD = \
    libabrt-problems2-service.a \
    $(GIO_LIBS) \
    $(GIO_UNIX_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(POLKIT_LIBS) \
    -labrt_dbus

abrt_configuration_SOURCES = \
    abrt-configuration.c \
    abrt-polkit.c \
    abrt-polkit.h
abrt_configuration_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPROBLEMS_CONFIG_INTERFACES_DIR=\"$(PROBLEMS_CONFIG_INTERFACES_DIR)\" \
    $(GIO_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
abrt_configuration_LDADD = \
    $(GIO_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(POLKIT_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

polkitconfdir = ${datadir}/polkit-1/actions
dist_polkitconf_DATA = abrt_polkit.policy

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

orgfreedesktopproblemsservicedir = ${datadir}/dbus-1/system-services
dist_orgfreedesktopproblemsservice_DATA = org.freedesktop.problems.service \
    com.redhat.problems.configuration.service

IFACE_DEF_DIR=$(abs_top_srcdir)/doc/problems-service/

PROBLEMS2_XML_FILES=\
    $(IFACE_DEF_DIR)/org.freedesktop.Problems2.xml \
    $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Entry.xml \
    $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Session.xml \
    $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Task.xml

.PHONY: $(IFACE_DEF_DIR)/org.freedesktop.Problems2.xml
$(IFACE_DEF_DIR)/org.freedesktop.Problems2.xml:
	$(MAKE) -C $(IFACE_DEF_DIR) $$(basename $@)

.PHONY: $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Entry.xml
$(IFACE_DEF_DIR)/org.freedesktop.Problems2.Entry.xml:
	$(MAKE) -C $(IFACE_DEF_DIR) $$(basename $@)

.PHONY: $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Session.xml
$(IFACE_DEF_DIR)/org.freedesktop.Problems2.Session.xml:
	$(MAKE) -C $(IFACE_DEF_DIR) $$(basename $@)

.PHONY: $(IFACE_DEF_DIR)/org.freedesktop.Problems2.Task.xml
$(IFACE_DEF_DIR)/org.freedesktop.Problems2.Task.xml:
	$(MAKE) -C $(IFACE_DEF_DIR) $$(basename $@)

.PHONY: abrt_problems2_generated_interfaces.h
abrt_problems2_generated_interfaces.h: $(PROBLEMS2_XML_FILES)
	echo "/* This file has been generated. Do not write any code here. */" > $@
	for f in $^; do echo "const char *g_$$(basename $$f .xml | tr . _)_xml =" >> $@; sed -e 's/"/\\"/g' -e 's/^\(.*\)$$/"\1"/' $$f >> $@; echo ";" >> $@; done

eventsconfdir = $(EVENTS_CONF_DIR)
