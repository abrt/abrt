Summary: Automatic bug detection and reporting tool
Name: crash-catcher
Version: 0.0.1
Release: 9%{?dist}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/crash-catcher/
Source: crash-catcher-0.0.1.tar.gz
Source1: crash-catcher.init
BuildRequires: dbus-c++-devel
BuildRequires: gtkmm24-devel
BuildRequires: dbus-glib-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: sqlite-devel > 3.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
CrashCatcher is a tool to help users to detect defects in applications and 
to create a bug report with all informations needed by maintainer to fix it. 
It uses plugin system to extend its functionality.

%package libs
Summary: Libraries for CrashCatcher
Group: System Environment/Libraries

%description libs
Libraries for CrashCatcher.

%package devel
Summary: Development libraries for CrashCatcher
Group: Development/Libraries
Requires: %{name}-libs = %{version}-%{release}

%description devel
Development libraries and headers for CrashCatcher.

%package applet
Summary: CrashCatcher's applet
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}

%description applet
Simple systray applet to notify user about new events detected by crash-catcher 
daemon.

%package gui
Summary: CrashCatcher's gui
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: CrashCatcher's C/C++ addon
Group: System Environment/Libraries
Requires: gdb
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and CrashCatcher's C/C++ 
language plugin.

%package plugin-sqlite3
Summary: CrashCatcher's SQLite3 database plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-sqlite3
This package contains SQLite3 database plugin. It is used for storing the data 
required for creating a bug report.

%package plugin-logger
Summary: CrashCatcher's logger reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-logger
The simple reporter plugin, which writes a report to a specified file.

%package plugin-mailx
Summary: CrashCatcher's mailx reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}
Requires: mailx

%description plugin-mailx
The simple reporter plugin, which sends a report via mailx to a specified
email. 

%prep
%setup -q

%build
%configure
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

rm -rf $RPM_BUILD_ROOT/%{_libdir}/lib*.la
rm -rf $RPM_BUILD_ROOT/%{_libdir}/crash-catcher/lib*.la
mkdir -p ${RPM_BUILD_ROOT}/%{_initrddir}
install -m 755 %SOURCE1 ${RPM_BUILD_ROOT}/%{_initrddir}/crash-catcher
mkdir -p $RPM_BUILD_ROOT/var/cache/crash-catcher

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/chkconfig --add crash-catcher

%post libs -p /sbin/ldconfig

%preun
if [ "$1" = 0 ] ; then
  service crash-catcher stop >/dev/null 2>&1
  /sbin/chkconfig --del crash-catcher
fi

%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_sbindir}/crash-catcher
%config(noreplace) %{_sysconfdir}/crash-catcher/crash-catcher.conf
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-crash-catcher.conf
%{_initrddir}/crash-catcher
%dir /var/cache/crash-catcher
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/plugins

%files libs
%defattr(-,root,root,-)
%{_libdir}/lib*.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/lib*.so

%files applet
%defattr(-,root,root,-)
%{_bindir}/cc-applet

%files gui
%defattr(-,root,root,-)
%{_bindir}/cc-gui
%{_datadir}/%{name}

%files addon-ccpp
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/CCpp.conf
%{_libdir}/crash-catcher/libCCpp.so*
%{_libexecdir}/hookCCpp

%files plugin-sqlite3
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/SQLite3.conf
%{_libdir}/crash-catcher/libSQLite3.so*

%files plugin-logger
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Logger.conf
%{_libdir}/crash-catcher/libLogger.so*

%files plugin-mailx
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Mailx.conf
%{_libdir}/crash-catcher/libMailx.so*

%changelog
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
