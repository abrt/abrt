SUBDIRS = plugins

bin_PROGRAMS = abrt-retrace-worker
abrt_retrace_worker_SOURCES = worker.c

retrace_PYTHON = retrace.py
retracedir = $(datadir)/abrt-retrace

worker_PYTHON = coredump2packages.py worker.py
workerdir = $(datadir)/abrt-retrace

cleanup_PYTHON = abrt-retrace-cleanup.py
cleanupdir = $(datadir)/abrt-retrace

reposync_PYTHON = abrt-retrace-reposync.py
reposyncdir = $(datadir)/abrt-retrace

interface_PYTHON = backtrace.wsgi create.wsgi log.wsgi settings.wsgi status.wsgi
# interfacedir should probably be $$(pkgdatadir)/retrace
interfacedir =  $(datadir)/abrt-retrace

repo_DATA = retrace.repo
repodir = ${sysconfdir}/yum.repos.d

retraceconf_DATA = retrace.conf
retraceconfdir = ${sysconfdir}/abrt

httpdconf_DATA = retrace_httpd.conf
httpdconfdir = ${sysconfdir}/httpd/conf.d

EXTRA_DIST = retrace.conf retrace_httpd.conf retrace.repo

# Apache config files can be owned by root, httpd just needs read
# access.
#install-data-hook:
#       chown apache:apache $(DESTDIR)$(httpdconfdir)/retrace_httpd.conf
