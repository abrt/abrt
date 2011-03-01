bin_PROGRAMS = abrt-retrace-worker
abrt_retrace_worker_SOURCES = worker.c

dist_bin_SCRIPTS=abrt-retrace-reposync

retrace_PYTHON = retrace.py
retracedir = $(datadir)/abrt-retrace

interface_PYTHON = backtrace.wsgi create.wsgi log.wsgi status.wsgi
# interfacedir should probably be $$(pkgdatadir)/retrace
interfacedir =  $(datadir)/abrt-retrace

repo_DATA = retrace.repo retrace-local.repo
repodir = ${sysconfdir}/yum.repos.d

retraceconf_DATA = retrace.conf
retraceconfdir = ${sysconfdir}/abrt

httpdconf_DATA = retrace_httpd.conf
httpdconfdir = ${sysconfdir}/httpd/conf.d

EXTRA_DIST = retrace.conf retrace_httpd.conf retrace.repo retrace-local.repo

# Apache config files can be owned by root, httpd just needs read
# access.
#install-data-hook:
#       chown apache:apache $(DESTDIR)$(httpdconfdir)/retrace_httpd.conf
