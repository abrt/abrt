retraceconf_DATA = retrace.conf
retraceconfdir = ${sysconfdir}/abrt

httpdconf_DATA = retrace_httpd.conf
httpdconfdir = ${sysconfdir}/httpd/conf.d

EXTRA_DIST = retrace.conf retrace_httpd.conf

# Apache config files can be owned by root, httpd just needs read
# access.
#install-data-hook:
#	chown apache:apache $(DESTDIR)$(httpdconfdir)/retrace_httpd.conf
