%{!?python_site: %define python_site %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(0)")}
# platform-dependent
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
%if 0%{?fedora} >= 14
    %define with_systemd 1
%else
    %define with_systemd 0
%endif

# please modify the "_buildid" define in a way that identifies
# that the built package isn't the stock distribution package,
# for example, by setting abbreviation sha1 hash "238f49f"
#
# % define _buildid git238f49f

%if "0%{?_buildid}" != "0"
%define pkg_release 0.%{?_buildid}%{?dist}
%else
%define pkg_release 1%{?dist}
%endif

Summary: Automatic bug detection and reporting tool
Name: abrt
Version: 2.0.0
Release: %{?pkg_release}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/abrt/
Source: https://fedorahosted.org/released/%{name}/%{name}-%{version}.tar.gz
Source1: abrt.init
Source2: abrt-ccpp.init
BuildRequires: dbus-devel
BuildRequires: gtk2-devel
BuildRequires: curl-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: desktop-file-utils
BuildRequires: libnotify-devel
BuildRequires: xmlrpc-c-devel
BuildRequires: file-devel
BuildRequires: python-devel
BuildRequires: gettext
BuildRequires: libxml2-devel
BuildRequires: libtar-devel
BuildRequires: intltool
BuildRequires: libtool
BuildRequires: nss-devel
BuildRequires: texinfo

# for rhel6
%if 0%{?rhel} >= 6
BuildRequires: gnome-keyring-devel
%else
BuildRequires: libgnome-keyring-devel
%endif

%if %{?with_systemd}
Requires: systemd-units
%endif
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: %{name}-libs = %{version}-%{release}
Requires: libreport
Requires(pre): shadow-utils
Obsoletes: abrt-plugin-sqlite3 > 0.0.1
# required for transition from 1.1.13, can be removed after some time
Obsoletes: abrt-plugin-runapp > 0.0.1
Obsoletes: abrt-plugin-filetransfer > 0.0.1
Obsoletes: abrt-plugin-sosreport > 0.0.1

%description
%{name} is a tool to help users to detect defects in applications and
to create a bug report with all informations needed by maintainer to fix it.
It uses plugin system to extend its functionality.

%package -n libreport
Summary: Libraries for reporting crashes to different targets.
Group: System Environment/Libraries

%description -n libreport
Libraries providing API for reporting different problems in applications
to different bug targets like bugzilla, ftp, trac, etc...

%package -n libreport-devel
Summary: Development libraries and headers for libreport.
Group: Development/Libraries

%description -n libreport-devel
Development libraries and headers for libreport.

%package -n libreport-python
Summary: Python bindings for report-libs.
# Is group correct here? -
Group: System Environment/Libraries

%description -n libreport-python
Python bindings for report-libs.

%package -n libreport-gtk
Summary: GTK frontend for libreport
Group: User Interface/Desktops

%description -n libreport-gtk
Applications for reporting bugs using libreport backend.

%package libs
Summary: Libraries for %{name}
Group: System Environment/Libraries

%description libs
Libraries for %{name}.

%package devel
Summary: Development libraries for %{name}
Group: Development/Libraries
Requires: abrt-libs = %{version}-%{release}

%description devel
Development libraries and headers for %{name}.

%package gui
Summary: %{name}'s gui
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}
Requires: libreport-gtk
# we used to have abrt-applet, now abrt-gui includes it:
Provides: abrt-applet = %{version}-%{release}
Obsoletes: abrt-applet < 0.0.5
Conflicts: abrt-applet < 0.0.5

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: %{name}'s C/C++ addon
Group: System Environment/Libraries
Requires: elfutils, cpio
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and %{name}'s C/C++
analyzer plugin.

%package addon-kerneloops
Summary: %{name}'s kerneloops addon
Group: System Environment/Libraries
Requires: curl
Requires: %{name} = %{version}-%{release}
Obsoletes: kerneloops > 0.0.1
Obsoletes: abrt-plugin-kerneloops > 0.0.1
Obsoletes: abrt-plugin-kerneloopsreporter > 0.0.1

%description addon-kerneloops
This package contains plugin for collecting kernel crash information
and reporter plugin which sends this information to specified server,
usually to kerneloops.org.

%package plugin-logger
Summary: %{name}'s logger reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-logger
The simple reporter plugin which writes a report to a specified file.

%package plugin-mailx
Summary: %{name}'s mailx reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}
Requires: mailx

%description plugin-mailx
The simple reporter plugin which sends a report via mailx to a specified
email address.

%package plugin-bugzilla
Summary: %{name}'s bugzilla plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-bugzilla
Plugin to report bugs into the bugzilla.

%package plugin-rhtsupport
Summary: %{name}'s RHTSupport plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}
Obsoletes: abrt-plugin-catcut > 0.0.1
Obsoletes: abrt-plugin-rhfastcheck > 0.0.1
Obsoletes: abrt-plugin-rhticket > 0.0.1

%description plugin-rhtsupport
Plugin to report bugs into RH support system.

%package plugin-reportuploader
Summary: %{name}'s reportuploader plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}
Obsoletes: abrt-plugin-ticketuploader > 0.0.1

%description plugin-reportuploader
Plugin to report bugs into anonymous FTP site associated with ticketing system.

%package addon-python
Summary: %{name}'s addon for catching and analyzing Python exceptions
Group: System Environment/Libraries
Requires: python
Requires: %{name} = %{version}-%{release}
Obsoletes: gnome-python2-bugbuddy > 0.0.1
Provides: gnome-python2-bugbuddy

%description addon-python
This package contains python hook and python analyzer plugin for handling
uncaught exception in python programs.

%package cli
Summary: %{name}'s command line interface
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}
Requires: abrt-addon-kerneloops
Requires: abrt-addon-ccpp, abrt-addon-python
Requires: abrt-plugin-bugzilla, abrt-plugin-logger

%description cli
This package contains simple command line client for controlling abrt daemon over
the sockets.

%package desktop
Summary: Virtual package to install all necessary packages for usage from desktop environment
Group: User Interface/Desktops
# This package gets installed when anything requests bug-buddy -
# happens when users upgrade Fn to Fn+1;
# or if user just wants "typical desktop installation".
# Installing abrt-desktop should result in the abrt which works without
# any tweaking in abrt.conf (IOW: all plugins mentioned there must be installed)
Requires: %{name} = %{version}-%{release}
Requires: abrt-addon-kerneloops
Requires: abrt-addon-ccpp, abrt-addon-python
# Default config of addon-ccpp requires gdb
Requires: gdb >= 7.0-3
Requires: abrt-gui
Requires: abrt-plugin-logger, abrt-plugin-bugzilla
#Requires: abrt-plugin-firefox
Obsoletes: bug-buddy > 0.0.1
Provides: bug-buddy

%description desktop
Virtual package to make easy default installation on desktop environments.

%package retrace-server
Summary: %{name}'s retrace server using HTTP protocol
Group: System Environment/Daemons
Requires: abrt-addon-ccpp
Requires: gdb >= 7.0-3
Requires: httpd, mod_wsgi, mod_ssl, python-webob
Requires: mock, xz, elfutils, createrepo, rsync
%{?el6:Requires: python-argparse}
Requires(preun): /sbin/install-info
Requires(post): /sbin/install-info

%description retrace-server
The retrace server provides a coredump analysis and backtrace
generation service over a network using HTTP protocol.

%prep
%setup -q

%build
autoconf
%configure
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
CFLAGS="-fno-strict-aliasing"
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT mandir=%{_mandir}
%find_lang %{name}

# remove all .la and .a files
find $RPM_BUILD_ROOT -name '*.la' -or -name '*.a' | xargs rm -f
mkdir -p ${RPM_BUILD_ROOT}/%{_initrddir}
install -m 755 %SOURCE1 ${RPM_BUILD_ROOT}/%{_initrddir}/abrtd
install -m 755 %SOURCE2 ${RPM_BUILD_ROOT}/%{_initrddir}/abrt-ccpp
mkdir -p $RPM_BUILD_ROOT/var/cache/abrt-di
mkdir -p $RPM_BUILD_ROOT/var/run/abrt
mkdir -p $RPM_BUILD_ROOT/var/spool/abrt
mkdir -p $RPM_BUILD_ROOT/var/spool/abrt-retrace
mkdir -p $RPM_BUILD_ROOT/var/cache/abrt-retrace
mkdir -p $RPM_BUILD_ROOT/var/log/abrt-retrace
mkdir -p $RPM_BUILD_ROOT/var/spool/abrt-upload

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_datadir}/applications \
        --vendor fedora \
        --delete-original \
        ${RPM_BUILD_ROOT}%{_datadir}/applications/%{name}.desktop

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_sysconfdir}/xdg/autostart \
        src/applet/abrt-applet.desktop

# After everything is installed, remove info dir
rm -f %{buildroot}%{_infodir}/dir

%clean
rm -rf $RPM_BUILD_ROOT

%pre
#uidgid pair 173:173 reserved in setup rhbz#670231
%define abrt_gid_uid 173
getent group abrt >/dev/null || groupadd -f -g %{abrt_gid_uid} --system abrt
getent passwd abrt >/dev/null || useradd --system -g abrt -u %{abrt_gid_uid} -d /etc/abrt -s /sbin/nologin abrt
exit 0

%post
if [ $1 -eq 1 ]; then
/sbin/chkconfig --add abrtd
fi
#systemd
%if %{?with_systemd}
#if [ $1 -eq 1 ]; then
# Enable (but don't start) the units by default
  /bin/systemctl enable abrtd.service >/dev/null 2>&1 || :
#fi
%endif

%post addon-ccpp
if [ $1 -eq 1 ]; then
/sbin/chkconfig --add abrt-ccpp
fi
#systemd: TODO

%post retrace-server
/sbin/install-info %{_infodir}/abrt-retrace-server %{_infodir}/dir 2> /dev/null || :
/usr/sbin/usermod -G mock apache 2> /dev/null || :

%preun
if [ "$1" -eq "0" ] ; then
  service abrtd stop >/dev/null 2>&1
  /sbin/chkconfig --del abrtd
fi
#systemd
%if %{?with_systemd}
if [ "$1" -eq "0" ] ; then
  /bin/systemctl stop abrtd.service >/dev/null 2>&1 || :
  /bin/systemctl disable abrtd.service >/dev/null 2>&1 || :
fi
%endif

%preun addon-ccpp
if [ "$1" -eq "0" ] ; then
  service abrt-ccpp stop >/dev/null 2>&1
  /sbin/chkconfig --del abrt-ccpp
fi
#systemd: TODO

%preun retrace-server
if [ "$1" = 0 ]; then
  /sbin/install-info --delete %{_infodir}/abrt-retrace-server %{_infodir}/dir 2> /dev/null || :
fi

%postun
#systemd
%if %{?with_systemd}
if [ $1 -ge 1 ] ; then
# On upgrade, reload init system configuration if we changed unit files
  /bin/systemctl daemon-reload >/dev/null 2>&1 || :
fi
%endif

%post gui
# update icon cache
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%postun gui
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi

%posttrans
if [ "$1" -eq "0" ]; then
    service abrtd condrestart >/dev/null 2>&1 || :
fi
#systemd
%if %{?with_systemd}
if [ "$1" -eq "0" ]; then
    /bin/systemctl try-restart abrtd.service >/dev/null 2>&1 || :
fi
%endif

%posttrans addon-ccpp
if [ "$1" -eq "0" ]; then
    service abrt-ccpp condrestart >/dev/null 2>&1 || :
fi
#systemd: TODO


%files -f %{name}.lang
%defattr(-,root,root,-)
%doc README COPYING
#systemd
%if %{?with_systemd}
/lib/systemd/system/abrtd.service
%endif
%{_sbindir}/abrtd
%{_sbindir}/abrt-server
%{_bindir}/abrt-handle-upload
%{_bindir}/abrt-handle-crashdump
%{_bindir}/abrt-action-save-package-data
%{_bindir}/abrt-retrace-client
%config(noreplace) %{_sysconfdir}/%{name}/abrt.conf
%config(noreplace) %{_sysconfdir}/%{name}/abrt_event.conf
%config(noreplace) %{_sysconfdir}/%{name}/gpg_keys
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-abrt.conf
%{_initrddir}/abrtd
%dir %attr(0755, abrt, abrt) %{_localstatedir}/spool/%{name}
%dir %attr(0700, abrt, abrt) %{_localstatedir}/spool/%{name}-upload
%dir %attr(0775, abrt, abrt) %{_localstatedir}/run/%{name}
%ghost %attr(0666, -, -) %{_localstatedir}/run/%{name}/abrt.socket
%ghost %attr(0644, -, -) %{_localstatedir}/run/abrtd.pid
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/plugins
%dir %{_sysconfdir}/%{name}/events.d
#%dir %{_libdir}/%{name}
%{_mandir}/man8/abrtd.8.gz
%{_mandir}/man5/%{name}.conf.5.gz
# {_mandir}/man5/pyhook.conf.5.gz
%{_mandir}/man7/%{name}-plugins.7.gz
%{_datadir}/dbus-1/system-services/com.redhat.abrt.service

%files -n libreport
%defattr(-,root,root,-)
%{_libdir}/libreport.so.*

%files -n libreport-devel
%defattr(-,root,root,-)
%{_includedir}/report/*
%{_libdir}/libreport.so

%files -n libreport-python
%defattr(-,root,root,-)
%{python_sitearch}/report/*

%files -n libreport-gtk
%defattr(-,root,root,-)
%{_bindir}/bug-reporting-wizard
%{_libdir}/libreportgtk.so.*

%files libs
%defattr(-,root,root,-)
%{_libdir}/libabrt*.so.*
%{_libdir}/libbtparser.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/abrt/*
%{_libdir}/libabrt*.so
%{_includedir}/btparser/*
%{_libdir}/libbtparser.so
#FIXME: this should go to libreportgtk-devel package
%{_libdir}/libreportgtk.so
%{_libdir}/pkgconfig/*
%doc doc/abrt-plugin doc/howto-write-reporter

%files gui
%defattr(-,root,root,-)
%{_bindir}/abrt-gui
%dir %{_datadir}/%{name}
# all glade, gtkbuilder and py files for gui
%{_datadir}/applications/fedora-%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/icons/hicolor/*/status/*
%{_datadir}/%{name}/icons/hicolor/*/status/*
%{_bindir}/abrt-applet
#%{_bindir}/test-report
%{_sysconfdir}/xdg/autostart/abrt-applet.desktop

%files addon-ccpp
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/CCpp.conf
%dir %attr(0775, abrt, abrt) %{_localstatedir}/cache/abrt-di
%{_initrddir}/abrt-ccpp
%{_libexecdir}/abrt-hook-ccpp
%{_bindir}/abrt-action-analyze-c
%{_bindir}/abrt-action-trim-files
%attr(2755, abrt, abrt) %{_bindir}/abrt-action-install-debuginfo
%{_bindir}/abrt-action-install-debuginfo.py*
%{_bindir}/abrt-action-generate-backtrace
%{_bindir}/abrt-action-analyze-backtrace
%{_bindir}/abrt-action-list-dsos.py*
%{_sysconfdir}/%{name}/events.d/ccpp_events.conf
%{_sysconfdir}/%{name}/events/analyze_LocalGDB.xml
%{_sysconfdir}/%{name}/events/reanalyze_LocalGDB.xml
%{_sysconfdir}/%{name}/events/analyze_RetraceServer.xml
%{_sysconfdir}/%{name}/events/reanalyze_RetraceServer.xml

%files addon-kerneloops
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Kerneloops.conf
%{_sysconfdir}/%{name}/events/report_Kerneloops.xml
%{_sysconfdir}/%{name}/events.d/koops_events.conf
%{_mandir}/man7/abrt-KerneloopsReporter.7.gz
%{_bindir}/abrt-dump-oops
%{_bindir}/abrt-action-analyze-oops
%{_bindir}/abrt-action-kerneloops

%files plugin-logger
%defattr(-,root,root,-)
%{_sysconfdir}/%{name}/events/report_Logger.conf
%{_mandir}/man7/abrt-Logger.7.gz
%{_bindir}/abrt-action-print

%files plugin-mailx
%defattr(-,root,root,-)
%{_sysconfdir}/%{name}/events/report_Mailx.xml
%{_sysconfdir}/%{name}/events.d/mailx_events.conf
%{_mandir}/man7/abrt-Mailx.7.gz
%{_bindir}/abrt-action-mailx

%files plugin-bugzilla
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Bugzilla.conf
%{_sysconfdir}/%{name}/events/report_Bugzilla.xml
%config(noreplace) %{_sysconfdir}/%{name}/events/report_Bugzilla.conf
# FIXME: remove with the old gui
%{_mandir}/man7/abrt-Bugzilla.7.gz
%{_bindir}/abrt-action-bugzilla

%files plugin-rhtsupport
%defattr(-,root,root,-)
%{_sysconfdir}/%{name}/events/report_RHTSupport.xml
# {_mandir}/man7/abrt-RHTSupport.7.gz
%{_bindir}/abrt-action-rhtsupport

%files plugin-reportuploader
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Upload.conf
%{_mandir}/man7/abrt-Upload.7.gz
%{_bindir}/abrt-action-upload

%files addon-python
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Python.conf
%{_bindir}/abrt-action-analyze-python
%{python_site}/*.py*
%{python_site}/abrt.pth

%files cli
%defattr(-,root,root,-)
%{_bindir}/abrt-cli
%{_mandir}/man1/abrt-cli.1.gz
%{_sysconfdir}/bash_completion.d/abrt-cli.bash

%files desktop
%defattr(-,root,root,-)

%files retrace-server
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/retrace.conf
%config(noreplace) %{_sysconfdir}/httpd/conf.d/retrace_httpd.conf
%config(noreplace) %{_sysconfdir}/yum.repos.d/retrace.repo
%dir %attr(0775, apache, abrt) %{_localstatedir}/spool/abrt-retrace
%dir %attr(0755, abrt, abrt) %{_localstatedir}/cache/abrt-retrace
%dir %attr(0755, abrt, abrt) %{_localstatedir}/log/abrt-retrace
%caps(cap_setuid=ep) %{_bindir}/abrt-retrace-worker
%{_datadir}/abrt-retrace/*.py*
%{_datadir}/abrt-retrace/plugins/*.py*
%{_datadir}/abrt-retrace/*.wsgi
%{_infodir}/abrt-retrace-server*

%changelog
* Wed Mar 16 2011 Jiri Moskovcak <jmoskovc@redhat.com> 2.0.0-1
- update to the latest upstream version
- many improvements
- FIXME: add closed bugzillas

* Fri Feb 18 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.17-2
- removed gnome-python2-vfs dependency

* Mon Feb 07 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.1.17-1.1
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sat Feb 05 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.17-1
- rewritten abrt-debuginfo-install script to use the yum API
- GUI: added search box to backtrace view rhbz#612017 (jmoskovc@redhat.com)
- fixed some gui warnings rhbz#671488 (jmoskovc@redhat.com)
- btparser/dupechecker improvements:
  - Better handling of glibc architecture-specific functions (kklic@redhat.com)
  - support format of thread header: "Thread 8 (LWP 6357):" (kklic@redhat.com)

* Fri Feb 04 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.16-1
- rhtsupport: added list of attachments to comment rhbz#668875
- rhtsupport: stop consuming non-standard header rhbz#670492
- Resolves: #670492, #668875

* Wed Jan 19 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.15-2
- add a gui/uid to useradd/groupadd command (reserved in setup rhbz#670231)
- Resolves: #650975

* Wed Jan 19 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.15-1
- removed unused files (jmoskovc@redhat.com)
- update po files (jmoskovc@redhat.com)
- removed some unused files (jmoskovc@redhat.com)
- pass old pattern to ccpp hook and use it (dvlasenk@redhat.com)
- GUI: added warning when gnome-keyring can't be accessed rhbz#576866 (jmoskovc@redhat.com)
- 666893 - Unable to make sense of XML-RPC response from server (npajkovs@redhat.com)
- PyHook: ignore SystemExit exception rhbz#636913 (jmoskovc@redhat.com)
- 665405 - ABRT's usage of sos does not grab /var/log/messages (npajkovs@redhat.com)
- add a note in report if kernel is tainted (npajkovs@redhat.com)
- KerneloopsScanner.cpp: make a room for NULL byte (npajkovs@redhat.com)
- fix multicharacter warring (npajkovs@redhat.com)
- open help page instead of about rhbz#666267

* Wed Jan 19 2011 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.14-3
- fixed build with rpm 4.9 (thx panu pmatilai for the patch)

* Wed Jan 19 2011 Matthias Clasen <mclasen@redhat.com> 1.1.14-2
- Rebuild against new rpm

* Wed Nov 17 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.14-1
- made howto mandatory
- fixed segv in abrt-hook-ccpp rhbz#652338
- added warning if kernel was tainted
- make the "install debuginfo" hint selectable rhbz#644343
- wrap howto and comments rhbz#625237
- wrap lines in the backtrace window rhbz#625232
- changed '*' to '•' rhbz#625236
- make the bt viewer not-editable rhbz#621871
- removed unneeded patches

* Wed Nov 10 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.13-3
- Rebuild for libnotify-0.7

* Wed Aug 25 2010 Jochen Schmitt <Jochen herr-schmitt de> 1.1.13-2
- Rebuild for python-2.7

* Tue Aug 10 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.13-1
- updated translation
- added native systemd file rhbz#617316 (jmoskovc@redhat.com)
- added ar to LINGUAS (jmoskovc@redhat.com)
- made /etc/abrt/plugins/Bugzilla.conf world-readable again (jmoskovc@redhat.com)
- l10n: adding fa locale (lashar@fedoraproject.org)
- l10n: new Persian (lashar@fedoraproject.org)
- remove libzip code (npajkovs@redhat.com)
- add libxml-2.0 into configure (npajkovs@redhat.com)
- fixed typo in man page rhbz#610748 (jmoskovc@redhat.com)
- RHTSupport: GUI's SSLVerify checkbox had one missing bit of code (vda.linux@googlemail.com)
- abrt_curl: discard headers from HTTP redirection (vda.linux@googlemail.com)
- moved abrt.socket and abrtd.lock into /var/run/abrt making selinux happy (jmoskovc@redhat.com)
- Mention --info and --backtrace in the abrt-cli man page. (kklic@redhat.com)
- build fixes for gcc 4.5 (jmoskovc@redhat.com)
- abrt-hook-ccpp: small fixes prompted by testing on RHEL5 (vda.linux@googlemail.com)
- Added --info action to abrt-cli (mtoman@redhat.com)
- wire up SSLVerify in RHTSupport.conf to actually have the desired effect (vda.linux@googlemail.com)
- fixed tooltip localization rhbz#574693 (jmoskovc@redhat.com)
- dumpoops/KerneloopsScanner: add pid to crashdump name (vda.linux@googlemail.com)
- A message change suggested by dhensley (kklic@redhat.com)

* Tue Jul 27 2010 David Malcolm <dmalcolm@redhat.com> - 1.1.10-4
- rebuild

* Tue Jul 27 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.10-3
- blacklist /usr/bin/nspluginviewer

* Mon Jul 26 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.10-2
- minor build fixes

* Mon Jul 26 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.10-1
- blacklisted mono-core package
- die with an error message if the database plugin is not accessible when needed (kklic@redhat.com)
- change RHTSupport URL protocol from HTTP to HTTPS (dvlasenk@redhat.com)
- the Logger plugin returns a message as the result of Report() call instead of a file URL (kklic@redhat.com)
- Cut off prelink suffixes from executable name if any (mtoman@redhat.com)
- CCpp: abrt-debuginfo-install output lines can be long, accomodate them (dvlasenk@redhat.com)
- do not pop up message on crash if the same crash is the same (dvlasenk@redhat.com)
- fedora bugs do not depend on rhel bugs (npajkovs@redhat.com)
- GUI: fixed problem with no gkeyring and just one reporter enabled rhbz#612457 (jmoskovc@redhat.com)
- added a document about interpreted language integration (kklic@redhat.com)
- moved devel header files to inc/ and included them in -devel package (jmoskovc@redhat.com, npajkovs@redhat.com)
- renamed abrt-utils.pc to abrt.pc (jmoskovc@redhat.com)
- string updates based on a UI text review (kklic@redhat.com)
- rhtsupport obsoletes the old rh plugins (jmoskovc@redhat.com)
- list allowed items in RHTSupport.conf (kklic@redhat.com)
- GUI: fixed package name in warning message when the packge is kernel rhbz#612191 (jmoskovc@redhat.com)
- remove rating for python crashes (jmoskovc@redhat.com)
- CCpp: give zero rating to an empty backtrace (jmoskovc@redhat.com)
- GUI: allow sending crashes without rating (jmoskovc@redhat.com)
- RHTSupport: set default URL to api.access.redhat.com/rs (dvlasenk@redhat.com)
- abort initialization on abrt.conf parsing errors (dvlasenk@redhat.com)
- changing NoSSLVerify to SSLVerify in bugzilla plugin (mtoman@redhat.com)
- added rating to python crashes
- show hostname in cli (kklic@redhat.com)
- updated po files (jmoskovc@redhat.com)
- added support for package specific actions rhbz#606917 (jmoskovc@redhat.com)
- renamed TicketUploader to ReportUploader (jmoskovc@redhat.com)
- bad hostnames on remote crashes (npajkovs@redhat.com)
- unlimited MaxCrashReportsSize (npajkovs@redhat.com)
- abrt_rh_support: improve error messages rhbz#608698 (vda.linux@googlemail.com)
- Added BacktraceRemotes option. (kklic@redhat.com)
- Allow remote crashes to not to belong to a package. Skip GPG check on remote crashes. (kklic@redhat.com)
- remove obsolete Catcut and rhfastcheck reporters (vda.linux@googlemail.com)
- make rhel bug point to correct place rhbz#578397 (npajkovs@redhat.com)
- Show comment and how to reproduce fields when reporing crashes in abrt-cli (kklic@redhat.com)
- Bash completion update (kklic@redhat.com)
- Rename --get-list to --list (kklic@redhat.com)
- Update man page (kklic@redhat.com)
- Options overhaul (kklic@redhat.com)
- abrt should not point to Fedora bugs but create new RHEL bug instead (npajkovs@redhat.com)
- Don't show global uuid in report (npajkovs@redhat.com)
- GUI: don't try to use action plugins as reporters (jmoskovc@redhat.com)
- Added WatchCrashdumpArchiveDir directive to abrt.conf and related code (vda.linux@googlemail.com)
- GUI: don't show the placehondler icon rhbz#605693 (jmoskovc@redhat.com)
- Make "Loaded foo.conf" message less confusing (vda.linux@googlemail.com)
- Fixed a flaw in strbuf_prepend_str (kklic@redhat.com)
- TicketUploader: do not add '\n' to text files in crashdump (vda.linux@googlemail.com)
- GUI: skip the plugin selection, if it's not needed (jmoskovc@redhat.com)
- Check conf file for syntax errors (kklic@redhat.com)
- move misplaced sanity checks in cron parser (vda.linux@googlemail.com)
- GUI: don't require the rating for all reporters (jmoskovc@redhat.com)
- GUI: fixed exception when there is no configure dialog for plugin rhbz#603745 (jmoskovc@redhat.com)
- Add a GUI config dialog for RHTSupport plugin (vda.linux@googlemail.com)
- abrt_curl: fix a problem with incorrect content-length on 32-bit arches (vda.linux@googlemail.com)
- sosreport: save the dump directly to crashdump directory (vda.linux@googlemail.com)
- plugin rename: rhticket -> RHTSupport (vda.linux@googlemail.com)
- Daemon socket for reporting crashes (karel@localhost.localdomain)
- GUI: fixed few typos (jmoskovc@redhat.com)

* Wed Jul 21 2010 David Malcolm <dmalcolm@redhat.com> - 1.1.5-1.1
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Wed Jun 09 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.5-1
- GUI: polished the reporter assistant (jmoskovc@redhat.com)
- Logger reporter: do not store useless info (vda.linux@googlemail.com)
- ccpp hook: add SaveBinaryImage option which saves of the crashed binary (vda.linux@googlemail.com)
- SPEC: added CFLAGS="-fno-strict-aliasing" to fix the rpmdiff warnings rhbz#599364 (jmoskovc@redhat.com)
- GUI: don't remove user comments when re-reporting the bug rhbz#601779 (jmoskovc@redhat.com)
- remove "(deleted)" from executable path rhbz#593037 (jmoskovc@redhat.com)
- CCpp analyzer: add 60 sec cap on gdb run time. (vda.linux@googlemail.com)
- add new file *hostname* into debugdump directory (npajkovs@redhat.com)
- rhticket: upload real tarball, not a bogus file (vda.linux@googlemail.com)
- abrt-hook-ccpp: eliminate race between process exit and compat coredump creation rhbz#584554 (vda.linux@googlemail.com)
- rhticket: actually do create ticket, using Gavin's lib code (vda.linux@googlemail.com)
- properly obsolete gnome-python2-bugbuddy rhbz#579748 (jmoskovc@redhat.com)
- GUI: remember comment and howto on backtrace refresh rhbz#545690 (jmoskovc@redhat.com)
- use header case in button label rhbz#565812 (jmoskovc@redhat.com)
- make log window resizable (vda.linux@googlemail.com)
- rename a few remaining /var/cache/abrt -> /var/spool/abrt (vda.linux@googlemail.com)

* Wed May 26 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.4-1
- added reporting wizard
- fixed few leaked fds
- fixed kerneloops --- cut here --- problem
- updated translations

* Fri May 21 2010 Denys Vlasenko <dvlasenk@redhat.com> 1.1.3-1
- More fixes for /var/cache/abrt -> /var/spool/abrt conversion

* Fri May 21 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.2-3
- fixed spec file to create /var/spool/abrt rhbz#593670
- updated init script to reflect the pid file renaming

* Wed May 19 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.2-1
- updated translation
- obsolete gnome-python2-bugbuddy rhbz#579748 (jmoskovc@redhat.com)
- Report "INFO: possible recursive locking detected rhbz#582378 (vda.linux@googlemail.com)
- kill yumdownloader if abrt-debuginfo-install is terminated mid-flight (vda.linux@googlemail.com)
- do not create Python dumps if argv[0] is not absolute (vda.linux@googlemail.com)
- improve kerneloops hash (vda.linux@googlemail.com)
- Move /var/cache/abrt to /var/spool/abrt. rhbz#568101. (vda.linux@googlemail.com)
- bugzilla: better summary and decription messages (npajkovs@redhat.com)
- renamed daemon pid and lock file rhbz#588315 (jmoskovc@redhat.com)
- Daemon socket for reporting crashes (kklic@redhat.com)
- Move hooklib from src/Hooks to lib/Utils (kklic@redhat.com)

* Thu May 13 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.1-1
- updated translations
- removed avant-window-navigator from blacklist (jmoskovc@redhat.com)
- Abort debuginfo download if low on disk space (partially addresses #564451) (vda.linux@googlemail.com)
- fix bug 588945 - sparse core files performance hit (vda.linux@googlemail.com)
- Add BlackListedPaths option to abrt.conf. Fixes #582421 (vda.linux@googlemail.com)
- Do not die when /var/cache/abrt/*/uid does not contain a number (rhbz#580899) (kklic@redhat.com)
- rid of rewriting config in /etc/abrt/abrt.conf (npajkovs@redhat.com)
- fix bug 571411: backtrace attachment of the form /var/cache/abrt/foo-12345-67890/backtrace (vda.linux@googlemail.com)
- Do not echo password to terminal in abrt-cli (kklic@redhat.com)
- improved daemon error messages (kklic@redhat.com)

* Mon May 03 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.1.0-1
- updated transaltions
- added Hebrew into languages
- updated icons rhbz#587698 (jmoskovc@redhat.com)
- Bugzilla login/password emptiness check uses 'or' instead of 'and' (kklic@redhat.com)
- Show error message when abrtd service is run as non-root. rhbz#584352 (kklic@redhat.com)
- Rename EnableOpenGPG to OpenGPGCheck in the man page rhbz#584332 (kklic@redhat.com)
- Document ProcessUnpackaged in abrt.conf.5. Document default values. (kklic@redhat.com)
- Crash function is now detected even for threads without an abort frame (kklic@redhat.com)
- comment can be private (npajkovs@redhat.com)
- do not catch perl/python crashes when the script is not of known package origin (kklic@redhat.com)
- kerneloop is more informative when failed (npajkovs@redhat.com)
- add function name into summary(if it's found) (npajkovs@redhat.com)
- Change kerneloops message when it fails (npajkovs@redhat.com)

* Fri Apr 30 2010 Karel Klic <kklic@redhat.com> 1.0.9-3
- fixed crash function detection (a part of duplication detection)

* Wed Apr 14 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.0.9-2
- fixed problem with localized yum messages rhbz#581804
- better bugzilla summary (napjkovs@redhat.com)
- ignore interpreter (py,perl) crashes caused by unpackaged scripts (kklic@redhat.com)

* Tue Apr 06 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.0.9-1
- hooklib: fix excessive rounding down in free space calculation (bz#575644) (vda.linux@googlemail.com)
- gui: fix 551989 "crash detected in abrt-gui-1.0.0-1.fc12" and such (vda.linux@googlemail.com)
- trivial: fix 566806 "abrt-gui sometimes can't be closed" (vda.linux@googlemail.com)
- gui: fix the last case where gnome-keyring's find_items_sync() may throw DeniedError (vda.linux@googlemail.com)
- fixed some compilation problems on F13 (jmoskovc@redhat.com)
- updated translations (jmoskovc@redhat.com)
- minor fix to sosreport to make it work with latest sos rhbz#576861 (jmoskovc@redhat.com)

* Wed Mar 31 2010 Jiri Moskovcak <jmoskovc@redhat.com> 1.0.9-0.201003312045.1
- test day build
- updated translation
- minor fix to sosreport to make it work with latest sos rhbz#576861 (jmoskovc@redhat.com)
- GUI: total rewrite based on design from Mairin Duffy (jmoskovc@redhat.com)
- trivial: better HTTP/curl error reporting (vda.linux@googlemail.com)
- Use backtrace parser from abrtutils, new backtrace rating algorithm, store crash function if it's known (kklic@redhat.com)
- abrt-rate-backtrace is replaced by abrt-backtrace --rate (kklic@redhat.com)
- Ignore some temp files (kklic@redhat.com)
- PYHOOK: don't use sitecustomize.py rhbz#539497 (jmoskovc@redhat.com)
- rhfastcheck: a new reporter plugin based on Gavin's work (vda.linux@googlemail.com)
- rhticket: new reporter plugin (vda.linux@googlemail.com)
- GUI: fixed few window icons (jmoskovc@redhat.com)
- Allow user to select which reporter he wants to use to report a crash using CLI.(kklic@redhat.com)
- bz reporter: s/uuid/duphash; more understandable message; simplify result str generation; fix indentation (vda.linux@googlemail.com)
- GUI: fixed crash count column sorting rhbz#573139 (jmoskovc@redhat.com)
- Kerneloops: use 1st line of oops as REASON. Closes rhbz#574196. (vda.linux@googlemail.com)
- Kerneloops: fix a case when we file an oops w/o backtrace (vda.linux@googlemail.com)
- minor fix in abrt-debuginfo-install to make it work with yum >= 3.2.26 (jmoskovc@redhat.com)
- GUI: added action to applet to directly report last crash (jmoskovc@redhat.com)
- Never flag backtrace as binary file (fixes problem observed in bz#571411) (vda.linux@googlemail.com)
- improve syslog file detection. closes bz#565983 (vda.linux@googlemail.com)
- add arch, package and release in comment (npajkovs@redhat.com)
- add ProcessUnpackaged option to abrt.conf (vda.linux@googlemail.com)
- abrt-debuginfo-install: use -debuginfo repos which match enabled "usual" repos (vda.linux@googlemail.com)
- fix format security error (fcrozat@mandriva.com)
- icons repackaging (jmoskovc@redhat.com)
- partial fix for bz#565983 (vda.linux@googlemail.com)
- SPEC: Updated source URL (jmoskovc@redhat.com)
- removed unneeded patches
- and much more ...

* Sat Mar 13 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.8-3
- fixed kerneloops reporting rhbz#570081
- fixed Source url
- fixed debuginfo-install to work on F13
  - improved debuginfo-install (vda.linux@googlemail.com)
  - fix debuginfo-install to work with yum >= 3.2.26 (jmoskovc@redhat.com)

* Wed Mar  3 2010  Denys Vlasenko <dvlasenk@redhat.com> 1.0.8-2
- fix initscript even more (npajkovs@redhat.com)
- remove -R2 from yum command line

* Mon Feb 22 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.8-1
- fix initscript (npajkovs@redhat.com)
- Kerneloops: make hashing more likely to produce same hash on different oopses (vda.linux@googlemail.com)

* Mon Feb 22 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.8-0.git-20100222
- Kerneloops: make hashing more likely to produce same hash on different oopses (vda.linux@googlemail.com)
- make abrt work with the latest kernels (>= 2.6.33) (jmoskovc@redhat.com)
- lib/Utils/abrt_dbus: utf8-sanitize all strings in dbus messages (fixes #565876) (vda.linux@googlemail.com)

* Fri Feb 12 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.7-1
- enabled column sorting rhbz#541853
- Load plugin settings also from ~/.abrt/*.conf (kklic@redhat.com)
- fix bz#541088 "abrt should not catch python excp EPIPE" (vda.linux@googlemail.com)
- fix bz#554242 "Cannot tab between input areas in report dialog" (vda.linux@googlemail.com)
- fix bz#563484 "abrt uses unnecessary disk space when getting debug info" (vda.linux@googlemail.com)
- Don't show empty 'Not loaded plugins' section - fix#2 rhbz#560971 (jmoskovc@redhat.com)
- fix big-endian build problem (vda.linux@googlemail.com)
- Fixes, displays package owners (kklic@redhat.com)
- GUI: fixed exception in plugin settings dialog rhbz#560851 (jmoskovc@redhat.com)
- GUI: respect system settings for toolbars rhbz#552161 (jmoskovc@redhat.com)
- python hook: move UUID generation to abrtd; generate REASON, add it to bz title (vda.linux@googlemail.com)
- make "reason" field less verbose; bz reporter: include it in "summary" (vda.linux@googlemail.com)
- added avant-window-navigator to blacklist per maintainer request (jmoskovc@redhat.com)
- CCpp analyzer: fix rhbz#552435 (bt rating misinterpreting # chars) (vda.linux@googlemail.com)
- Ask for login and password if missing from reporter plugin. (kklic@redhat.com)
- abrtd: fix handling of dupes (weren't deleting dup's directory); better logging (vda.linux@googlemail.com)
- abrtd: handle "perl -w /usr/bin/script" too (vda.linux@googlemail.com)
- Component-wise duplicates (kklic@redhat.com)
- abrtd: fix rhbz#560642 - don't die on bad plugin names (vda.linux@googlemail.com)
- Fixed parsing backtrace from rhbz#549293 (kklic@redhat.com)
- GUI: fixed scrolling in reporter dialog rhbz#559687 (jmoskovc@redhat.com)
- fixed button order in plugins windows rhbz#560961 (jmoskovc@redhat.com)
- GUI: fixed windows icons and titles rhbz#537240, rhbz#560964 (jmoskovc@redhat.com)
- Fix to successfully parse a backtrace from rhbz#550642 (kklic@redhat.com)
- cli: fix the problem of not showing oops text in editor (vda.linux@googlemail.com)
- GUI: fix rhbz#560971 "Don't show empty 'Not loaded plugins' section" (vda.linux@googlemail.com)

* Tue Feb  2 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.6-1
- print __glib_assert_msg (rhbz#549735);
- SPEC: added some requires to abrt-cli to make it work out-of-the-box (jmoskovc@redhat.com)
- abrt-hook-ccpp: fix rhbz#560612 "limit '18446744073709551615' is bogus" rhbz#560612(vda.linux@googlemail.com)
- APPLET: don't show the icon when abrtd is not running rhbz#557866 (jmoskovc@redhat.com)
- GUI: made report message labels wrap (jmoskovc@redhat.com)
- GUI: don't die if daemon doesn't send the gpg keys (jmoskovc@redhat.com)
- disabled the autoreporting of kerneloopses (jmoskovc@redhat.com)
- Kerneloops: fix BZ reporting of oopses (vda.linux@googlemail.com)
- GUI: wider report message dialog (jmoskovc@redhat.com)
- moved the gpg key list from abrt.conf to gpg_keys file (jmoskovc@redhat.com)
- Logger: create log file with mode 0600 (vda.linux@googlemail.com)
- GUI: fixed the rating logic, to prevent sending BT with rating < 3 (jmoskovc@redhat.com)
- Report GUI: made more fields copyable - closed rhbz#526209; tweaked wording (vda.linux@googlemail.com)
- GUI: fixed bug caused by failed gk-authorization (jmoskovc@redhat.com)

* Fri Jan 29 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.5-1
- moved the gpg key list from abrt.conf to gpg_keys file (jmoskovc@redhat.com)
- Logger: create log file with mode 0600 rhbz#559545 (vda.linux@googlemail.com)
- GUI: fixed the rating logic, to prevent sending BT with rating < 3 (jmoskovc@redhat.com)
- Report GUI: made more fields copyable - closed rhbz#526209; tweaked wording (vda.linux@googlemail.com)
- GUI: fixed bug caused by failed gk-authorization (jmoskovc@redhat.com)
- fix bug 559881 (kerneloops not shown in "new" GUI) (vda.linux@googlemail.com)
- GUI ReporterDialog: hide log button (vda.linux@googlemail.com)
- added valgrind and strace to blacklist (jmoskovc@redhat.com)
- SOSreport: do not leave stray files in /tmp (vda.linux@googlemail.com)
- Save the core where it belongs if ulimit -c is > 0 (jmoskovc@redhat.com)
- reenabled gpg check (jmoskovc@redhat.com)
- SOSreport: run it niced (vda.linux@googlemail.com)
- report GUI: rename buttons: Log -> Show log, Send -> Send report (vda.linux@googlemail.com)
- applet: reduce blinking timeout to 3 sec (vda.linux@googlemail.com)
- fix dbus autostart (vda.linux@googlemail.com)
- abrtd: set "Reported" status only if at least one reporter succeeded (vda.linux@googlemail.com)
- SQLite3: disable newline escaping, SQLite does not handle it (vda.linux@googlemail.com)
- SOSreport: make it avoid double runs; add forced regeneration; upd PLUGINS-HOWTO (vda.linux@googlemail.com)
- attribute SEGVs in perl to script's package, like we already do for python (vda.linux@googlemail.com)

* Wed Jan 20 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.4-1
- enabled sosreport
- fixes in ticketuploader
- GUI: redesign of reporter dialog (jmoskovc@redhat.com)
- Set the prgname to "Automatic Bug Reporting Tool" fixes rhbz#550357 (jmoskovc@redhat.com)
- CCpp analyzer: display __abort_msg in backtrace. closes rhbz#549735 (vda.linux@googlemail.com)
- s/os.exit/sys.exit - closes rhbz#556313 (vda.linux@googlemail.com)
- use repr() to print variable values in python hook rhbz#545070 (jmoskovc@redhat.com)
- gui: add logging infrastructure (vda.linux@googlemail.com)
- Added "Enabled = yes" to all plugin's config files (jmoskovc@redhat.com)
- *: disable plugin loading/unloading through GUI. Document keyring a bit (vda.linux@googlemail.com)
- fix memory leaks in catcut plugin (npajkovs@redhat.com)
- fix memory leaks in bugzilla (npajkovs@redhat.com)
- abrt-hook-python: sanitize input more; log to syslog (vda.linux@googlemail.com)
- Fixed /var/cache/abrt/ permissions (kklic@redhat.com)
- Kerneloops: we require commandline for every crash, save dummy one for oopses (vda.linux@googlemail.com)
- *: remove nss dependencies (vda.linux@googlemail.com)
- CCpp: use our own sha1 implementation (less pain with nss libs) (vda.linux@googlemail.com)
- DebugDump: more consistent logic in setting mode and uid:gid on dump dir (vda.linux@googlemail.com)
- fixes based on security review (vda.linux@googlemail.com)
- SOSreport/TicketUploader: use more restrictive file modes (vda.linux@googlemail.com)
- abrt-hook-python: add input sanitization and directory size guard (vda.linux@googlemail.com)
- RunApp: safer chdir. Overhauled "sparn a child and get its output" in general (vda.linux@googlemail.com)
- DebugDump: use more restrictive modes (vda.linux@googlemail.com)
- SQLite3: check for SQL injection (vda.linux@googlemail.com)
- replace plugin enabling via EnabledPlugins by par-plugin Enabled = yes/no (vda.linux@googlemail.com)
- abrt.spec: move "requires: gdb" to abrt-desktop (vda.linux@googlemail.com)
- ccpp: add a possibility to disable backtrace generation (vda.linux@googlemail.com)
- abrtd: limit the number of frames in backtrace to 3000 (vda.linux@googlemail.com)

* Tue Jan  5 2010  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.3-1
- speed optimalization of abrt-debuginfo-install (jmoskovc@redhat.com)
- updated credits (jmoskovc@redhat.com)
- GUI: fixed crash when abrt-gui is run without X server rhbz#552039 (jmoskovc@redhat.com)
- abrt-backtrace manpage installed (kklic@redhat.com)
- cmdline and daemon checking is done by abrt-python-hook (kklic@redhat.com)
- moved get_cmdline() and daemon_is_ok() to abrtlib (kklic@redhat.com)
- large file support for whole abrt (kklic@redhat.com)
- made s_signal_caught volatile (vda.linux@googlemail.com)
- abrt-debuginfo-install: fixes for runs w/o cachedir (vda.linux@googlemail.com)
- remove unsafe log() from signal handler (vda.linux@googlemail.com)
- src/Hooks/CCpp.cpp: use and honour 'c' (core limit size). (vda.linux@googlemail.com)
- lib/Plugins/CCpp.cpp: save gdb error messages too (vda.linux@googlemail.com)
- prevent destructors from throwing exceptions; check curl_easy_init errors (vda.linux@googlemail.com)
- don't blame python for every crash in /usr/bin/python rhbz#533521 trac#109 (jmoskovc@redhat.com)
- GUI: autoscroll log window (jmoskovc@redhat.com)
- Kerneloops.conf: better comments (vda.linux@googlemail.com)
- applet: reduce blinking time to 30 seconds (vda.linux@googlemail.com)
- add paranoia checks on setuid/setgid (vda.linux@googlemail.com)
- more "obviously correct" code for secure opening of /dev/null (vda.linux@googlemail.com)
- get rid of ugly sleep call inside while() (vda.linux@googlemail.com)

* Mon Dec 14 2009  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.2-1
- disabled GPG check again (jmoskovc@redhat.com)
- abrt-pyhook-helper rename (vda.linux@googlemail.com)
- abrt-cli: report success/failure of reporting. closes bug 71 (vda.linux@googlemail.com)
- less logging (vda.linux@googlemail.com)
- mkde abrt-gui --help and --version behave as expected. closes bug 85 (vda.linux@googlemail.com)
- dbus lib: fix parsing of 0-element arrays. Fixes bug 95 (vda.linux@googlemail.com)
- make "abrt-cli --delete randomuuid" report that deletion failed. closes bug 59 (vda.linux@googlemail.com)
- applet: make animation stop after 1 minute. (closes bug 108) (vda.linux@googlemail.com)
- show comment and how to reproduce fields, when BT rating > 3 (jmoskovc@redhat.com)
- Gui: make report status window's text wrap. Fixes bug 82 (vda.linux@googlemail.com)
- CCpp analyzer: added "info sharedlib" (https://fedorahosted.org/abrt/ticket/90) (vda.linux@googlemail.com)
- added link to bugzilla new account page to Bugzilla config dialog (jmoskovc@redhat.com)
- GUI: added log window (jmoskovc@redhat.com)

* Tue Dec  8 2009  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.1-1
- PyHook: better logic for checking if abrtd is running rhbz#539987 (jmoskovc@redhat.com)
- re-enabled gpg sign checking (jmoskovc@redhat.com)
- PyHook: use repr() for displaying variables rhbz#545070 (jmoskovc@redhat.com)
- kerneloops: fix the linux kernel version identification (aarapov@redhat.com)
- gui review (rrakus@redhat.com)
- when we trim the dir, we must delete it from DB too rhbz#541854 (vda.linux@googlemail.com)
- improved dupe checking (kklic@redhat.com)
- GUI: handle cases when gui fails to start daemon on demand rhbz#543725 (jmoskovc@redhat.com)
- Add abrt group only if it is missing; fixes rhbz#543250 (kklic@redhat.com)
- GUI: more string fixes rhbz#543266 (jmoskovc@redhat.com)
- abrt.spec: straighten out relations between abrt-desktop and abrt-gui (vda.linux@googlemail.com)
- refuse to start if some required plugins are missing rhbz#518422 (vda.linux@googlemail.com)
- GUI: survive gnome-keyring access denial rhbz#543200 (jmoskovc@redhat.com)
- typo fixes rhbz#543266 (jmoskovc@redhat.com)
- abrt-debuginfo-install: better fix for incorrect passing double quotes (vda.linux@googlemail.com)
- APPLET: stop animation when it's not needed rhbz#542157 (jmoskovc@redhat.com)
- ccpp hook: reanme it, and add "crash storm protection" (see rhbz#542003) (vda.linux@googlemail.com)
- Hooks/CCpp.cpp: add MakeCompatCore = yes/no directive. Fixes rhbz#541707 (vda.linux@googlemail.com)
- SPEC: removed sqlite3 package, fixed some update problems (jmoskovc@redhat.com)
- Kerneloops are reported automaticky now when AutoReportUIDs = root is in Kerneloops.conf (npajkovs@redhat.com)
- remove word 'detected' from description rhbz#541459 (vda.linux@googlemail.com)
- src/Hooks/CCpp.cpp: do save abrtd's own coredumps, but carefully... (vda.linux@googlemail.com)
- CCpp.cpp: quote parameters if needed rhbz#540164 (vda.linux@googlemail.com)

* Fri Nov 20 2009  Jiri Moskovcak <jmoskovc@redhat.com> 1.0.0-1
- new version
- comment input wraps words rhbz#531276
- fixed hiding password dialog rhbz#529583
- easier kerneloops reporting rhbz#528395
- made menu entry translatable rhbz#536878 (jmoskovc@redhat.com)
- GUI: don't read the g-k every time we want to use the setting (jmoskovc@redhat.com)
- GUI: survive if g-k access is denied rhbz#534171 (jmoskovc@redhat.com)
- include more info into oops (we were losing the stack dump) (vda.linux@googlemail.com)
- make BZ insert small text attachments inline; move text file detection code (vda.linux@googlemail.com)
- GUI: fixed text wrapping in comment field rhbz#531276 (jmoskovc@redhat.com)
- GUI: added cancel to send dialog rhbz#537238 (jmoskovc@redhat.com)
- include abrt version in bug descriptions (vda.linux@googlemail.com)
- ccpp hook: implemented ReadonlyLocalDebugInfoDirs directive (vda.linux@googlemail.com)
- GUI: added window icon rhbz#537240 (jmoskovc@redhat.com)
- add support for \" escaping in config file (vda.linux@googlemail.com)
- add experimental saving of /var/log/Xorg*.log for X crashes (vda.linux@googlemail.com)
- APPLET: changed icon from default gtk-warning to abrt specific, add animation (jmoskovc@redhat.com)
- don't show icon on abrtd start/stop rhbz#537630 (jmoskovc@redhat.com)
- /var/cache/abrt permissions 1775 -> 0775 in spec file (kklic@redhat.com)
- Daemon properly checks /var/cache/abrt attributes (kklic@redhat.com)
- abrt user group; used by abrt-pyhook-helper (kklic@redhat.com)
- pyhook-helper: uid taken from system instead of command line (kklic@redhat.com)
- KerneloopsSysLog: fix breakage in code which detects abrt marker (vda.linux@googlemail.com)
- GUI: added support for backtrace rating (jmoskovc@redhat.com)
- InformAllUsers support. enabled by default for Kerneloops. Tested wuth CCpp. (vda.linux@googlemail.com)
- abrtd: call res_init() if /etc/resolv.conf or friends were changed rhbz#533589 (vda.linux@googlemail.com)
- supress errors in python hook to not colide with the running script (jmoskovc@redhat.com)

* Tue Nov 10 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.11-2
- spec file fixes

* Mon Nov  2 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.11-1
- re-enabled kerneloops
- abrt-debuginfo-install: download packages one-by-one - better logging (vda.linux@googlemail.com)
- do not report empty fields (vda.linux@googlemail.com)
- Added abrt.png, fixed rhbz#531181 (jmoskovc@redhat.com)
- added option DebugInfoCacheMB to limit size of unpacked debuginfos (vda.linux@googlemail.com)
- fixed the problem with overwriting the default plugin settings (jmoskovc@redhat.com)
- disabled kerneloops in config file (jmoskovc@redhat.com)
- added dependency to gdb >= 7.0 (jmoskovc@redhat.com)
- better format of report text (vda.linux@googlemail.com)
- Python backtrace size limited to 1 MB (kklic@redhat.com)
- lib/Plugins/Bugzilla: better message at login failure (vda.linux@googlemail.com)
- build fixes, added plugin-logger to abrt-desktop (jmoskovc@redhat.com)
- blacklisted nspluginwrapper, because it causes too many useless reports (jmoskovc@redhat.com)
- GUI: Wrong settings window is not shown behind the reporter dialog rhbz#531119 (jmoskovc@redhat.com)
- Normal user can see kerneloops and report it Bugzilla memory leaks fix (npajkovs@redhat.com)
- dumpoops: add -s option to dump results to stdout (vda.linux@googlemail.com)
- removed kerneloops from abrt-desktop rhbz#528395 (jmoskovc@redhat.com)
- GUI: fixed exception when enabling plugin rhbz#530495 (jmoskovc@redhat.com)
- Improved abrt-cli (kklic@redhat.com)
- Added backtrace rating to CCpp analyzer (dnovotny@redhat.com)
- GUI improvements (jmoskovc@redhat.com)
- Added abrt-pyhook-helper (kklic@redhat.com)

* Thu Oct 15 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.10-1
- new version
- added more logging (vda.linux@googlemail.com)
- made polkit policy to be more permissive when installing debuginfo (jmoskovc@redhat.com)
- lib/Plugins/CCpp.cpp: add build-ids to backtrace (vda.linux@googlemail.com)
- lib/Plugins/CCpp.cpp: do not use temp file for gdb commands - use -ex CMD instead (vda.linux@googlemail.com)
- GUI: added refresh button, added sanity check to plugin settings (jmoskovc@redhat.com)
- Initial man page for abrt-cli (kklic@redhat.com)
- Added --version, -V, --help, -? options. Fixed crash caused by unknown option. (kklic@redhat.com)
- Date/time honors current system locale (kklic@redhat.com)
- fixed saving/reading user config (jmoskovc@redhat.com)
- SPEC: added gnome-python2-gnomekeyring to requirements (jmoskovc@redhat.com)
- GUI: call Report() with the latest pluginsettings (jmoskovc@redhat.com)
- Fix Bug 526220 -  [abrt] crash detected in abrt-gui-0.0.9-2.fc12 (vda.linux@googlemail.com)
- removed unsecure reading/writting from ~HOME directory rhbz#522878 (jmoskovc@redhat.com)
- error checking added to archive creation (danny@rawhide.localdomain)
- try using pk-debuginfo-install before falling back to debuginfo-install (vda.linux@googlemail.com)
- abrt-gui: make "report" toolbar button work even if abrtd is not running (vda.linux@googlemail.com)
- set LIMIT_MESSAGE to 16k, typo fix and daemon now reads config information from dbus (npajkovs@redhat.com)
- add support for abrtd autostart (vda.linux@googlemail.com)
- GUI: reversed the dumplist, so the latest crashes are at the top (jmoskovc@redhat.com)
- rewrite FileTransfer to use library calls instead of commandline calls for compression (dnovotny@redhat.com)
- and many minor fixes ..

* Wed Sep 23 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.9-2
- added bug-buddy to provides rhbz#524934

* Tue Sep 22 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.9-1
- new version
- comments and how to reproduce are stored now (npajkovs@redhat.com)
- reduce verbosity a bit (vda.linux@googlemail.com)
- GUI: fixed word wrap in Comment field rhbz#524349 (jmoskovc@redhat.com)
- remove last vestives of dbus-c++ from build system (vda.linux@googlemail.com)
- GUI: added popup menu, fixed behaviour when run with root privs (jmoskovc@redhat.com)
- add dbus signalization when quota exceeded (npajkovs@redhat.com)
- Added cleaning of attachment variable, so there should not be mixed attachmetn anymore. (zprikryl@redhat.com)
- fixed closing of debug dump in case of existing backtrace (zprikryl@redhat.com)
- remove C++ dbus glue in src/CLI; fix a bug in --report (vda.linux@googlemail.com)
- new polkit action for installing debuginfo, default "yes" (danny@rawhide.localdomain)
- Polkit moved to Utils (can be used both in daemon and plugins) (danny@rawhide.localdomain)
- oops... remove stray trailing '\' (vda.linux@googlemail.com)
- GUI: added missing tooltips (jmoskovc@redhat.com)
- PYHOOK: ignore KeyboardInterrupt exception (jmoskovc@redhat.com)
- added ticket uploader plugin (gavin@redhat.com) (zprikryl@redhat.com)
- GUI: added UI for global settings (just preview, not usable!) (jmoskovc@redhat.com)
- Add checker if bugzilla login and password are filled in. (npajkovs@redhat.com)
- Add new config option InstallDebuginfo into CCpp.conf (npajkovs@redhat.com)
- translation updates
- many other fixes

* Fri Sep  4 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.8.5-1
- new version
- APPLET: added about dialog, removed popup, if icon is not visible, fixed (trac#43) (jmoskovc@redhat.com)
- renamed abrt to abrtd, few minor spec file fixes (jmoskovc@redhat.com)
- Made abrt service start by deafult (jmoskovc@redhat.com)
- add gettext support for all plugins (npajkovs@redhat.com)
- APPLET: removed the warning bubble about not running abrt service (walters)
- APPLET: changed tooltip rhbz#520293 (jmoskovc@redhat.com)
- CommLayerServerDBus: rewrote to use dbus, not dbus-c++ (vda.linux@googlemail.com)
- fixed timeout on boot causing [ FAILED ] message (vda.linux@googlemail.com)
- and many other fixes

* Wed Sep 02 2009  Colin Walters <watlers@verbum.org> 0.0.8-2
- Change Conflicts: kerneloops to be an Obsoletes so we do the right thing
  on upgrades.  Also add an Obsoletes: bug-buddy.

* Wed Aug 26 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.8-1
- new version
- resolved: Bug 518420 -  ordinary user's abrt-applet shows up for root owned crashes (npajkovs)
- GUI: added support for gettext (+part of czech translation) (jmoskovc)
- added support for saving settings (zprikryl)
- fixed conf: comment in the middle of the line isn't supported anymore (zprikryl)
- BZ#518413 PATCH ... furious kerneloops reporting (aarapov)
- GUI: added first part of support for gettext (jmoskovc)
- add new parameter to FileTransfer plugin (dnovotny)
- added support for updating abrt's table (zprikryl)
- added check for cc-list and reporter. +1 is created iff reporter is somebody else and current user isn't in cc list. (zprikryl)
- GUI: few improvements, to be more userfriendly (jmoskovc)
- LOGGER: return valid uri of the log file on succes (jmoskovc)
- GUI: bring the GUI up to front instead of just blinking in taskbar (trac#60, rhbz#512390) (jmoskovc)
- Try to execute $bindir/abrt-gui, then fall back to $PATH search. Closes bug 65 (vda.linux)
- APPLET: added popup menu (trac#37, rhbz#518386) (jmoskovc)
- Improved report results (zprikryl)
- Fixed sigsegv (#rhbz 518609) (zprikryl)
- GUI: removed dependency on libsexy if gtk2 >= 2.17 (jmoskovc)
- fixed signature check (zprikryl)
- KerneloopsSysLog: check line length to be >= 4 before looking for "Abrt" (vda.linux)
- Comment cannot start in the middle of the line. Comment has to start by Char # (first char in the line) (zprikryl)
- command mailx isn't run under root anymore. (zprikryl)
- GUI: added horizontal scrolling to report window (jmoskovc)
- GUI: added clickable link to "after report" status window (jmoskovc)
- added default values for abrt daemon (zprikryl)
- Plugins/CCpp: remove trailing \n from debuginfo-install's output (vda.linux)
- explain EnableGPGCheck option better (vda.linux)
- mailx: correct English (vda.linux)
- Bugzilla.conf: correct English (vda.linux)
- GUI: nicer after report message (jmoskovc)
- BZ plugin: removed /xmlrpc.cgi from config, made the report message more user friendly (jmoskovc)
- CCpp plugin: do not abort if debuginfos aren't found (vda.linux)
- abrt.spec: bump version to 0.0.7-2 (vda.linux)
- mailx removed dangerous parameter option (zprikryl)
- minimum timeout is 1 second (zprikryl)
- in case of plugin error, don't delete debug dumps (zprikryl)
- abrt-gui: fix crash when run by root (vda.linux)
- and lot more in git log ...

* Thu Aug 20 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.7.2-1
- new version
- fixed some bugs found during test day

* Wed Aug 19 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.7.1-1
- fixes to bugzilla plugin and gui to make the report message more user-friendly

* Tue Aug 18 2009  Denys Vlasenko <dvlasenk@redhat.com> 0.0.7-2
- removed dangerous parameter option
- minimum plugin activation period is 1 second
- in case of plugin error, don't delete debug dumps
- abrt-gui: fix crash when run by root
- simplify parsing of debuginfo-install output

* Tue Aug 18 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.7-1
- new version
- added status window to show user some info after reporting a bug

* Mon Aug 17 2009  Denys Vlasenko <dvlasenk@redhat.com> 0.0.6-1
- new version
- many fixes

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.0.4-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Thu Jun 25 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.4-3
- fixed dependencies in spec file

* Tue Jun 16 2009 Daniel Novotny <dnovotny@redhat.com> 0.0.4-2
- added manual pages (also for plugins)

* Mon Jun 15 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.4-1
- new version
- added cli (only supports sockets)
- added python hook
- many fixes

* Fri Apr 10 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.3-1
- new version
- added bz plugin
- minor fix in reporter gui
- Configurable max size of debugdump storage rhbz#490889
- Wrap lines in report to keep the window sane sized
- Fixed gui for new daemon API
- removed unneeded code
- removed dependency on args
- new guuid hash creating
- fixed local UUID
- fixed debuginfo-install checks
- renamed MW library
- Added notification thru libnotify
- fixed parsing settings of action plugins
- added support for action plugins
- kerneloops - plugin: fail gracefully.
- Added commlayer to make dbus optional
- a lot of kerneloops fixes
- new approach for getting debuginfos and backtraces
- fixed unlocking of a debugdump
- replaced language and application plugins by analyzer plugin
- more excetpion handling
- conf file isn't needed
- Plugin's configuration file is optional
- Add curl dependency
- Added column 'user' to the gui
- Gui: set the newest entry as active (ticket#23)
- Delete and Report button are no longer active if no entry is selected (ticket#41)
- Gui refreshes silently (ticket#36)
- Added error reporting over dbus to daemon, error handling in gui, about dialog

* Wed Mar 11 2009  Jiri Moskovcak <jmoskovc@redhat.com> 0.0.2-1
- added kerneloops addon to rpm (aarapov)
- added kerneloops addon and plugin (aarapov)
- Made Crash() private
- Applet requires gui, removed dbus-glib deps
- Closing stdout in daemon rhbz#489622
- Changed applet behaviour according to rhbz#489624
- Changed gui according to rhbz#489624, fixed dbus timeouts
- Increased timeout for async dbus calls to 60sec
- deps cleanup, signal AnalyzeComplete has the crashreport as an argument.
- Fixed empty package Description.
- Fixed problem with applet tooltip on x86_64

* Wed Mar  4 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-13
- More renaming issues fixed..
- Changed BR from gtkmm24 to gtk2
- Fixed saving of user comment
- Added a progress bar, new Comment entry for user comments..
- FILENAME_CMDLINE and FILENAME_RELEASE are optional
- new default path to DB
- Rename to abrt

* Tue Mar  3 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-12
- initial fedora release
- changed SOURCE url
- added desktop-file-utils to BR
- changed crash-catcher to %%{name}

* Mon Mar  2 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-11
- more spec file fixes according to review
- async dbus method calls, added exception handler
- avoid deadlocks (zprikryl)
- root is god (zprikryl)
- create bt only once (zprikryl)

* Sat Feb 28 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-10
- New gui
- Added new method DeleteDebugDump to daemon
- Removed gcc warnings from applet
- Rewritten CCpp hook and removed dealock in DebugDumps lib (zprikryl)
- fixed few gcc warnings
- DBusBackend improvements

* Fri Feb 27 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-9
- fixed few gcc warnings
- added scrolled window for long reports

* Thu Feb 26 2009 Adam Williamson <awilliam@redhat.com> 0.0.1-8
- fixes for all issues identified in review

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-7
- Fixed cancel button behaviour in reporter
- disabled core file sending
- removed some debug messages

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-6
- fixed DB path
- added new signals to handler
- gui should survive the dbus timeout

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-5
- fixed catching debuinfo install exceptions
- some gui fixes
- added check for GPGP public key

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-4
- changed from full bt to simple bt

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-3
- spec file cleanups
- changed default paths to crash DB and log DB
- fixed some memory leaks

* Tue Feb 24 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-2
- spec cleanup
- added new subpackage gui

* Wed Feb 18 2009 Zdenek Prikryl <zprikryl@redhat.com> 0.0.1-1
- initial packing
