Summary: Automatic bug detection and reporting tool
Name: crash-catcher
Version: 0.0.1
Release: 2%{?dist}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/crash-catcher/
Source: crash-catcher-0.0.1.tar.gz
BuildRequires: dbus-c++-devel
BuildRequires: gtkmm24-devel
BuildRequires: glib2-devel
BuildRequires: dbus-glib-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: sqlite-devel > 3.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
CrashCatcher is a tool to help users to detect defects in applications and 
to create a bug report with all informations needed by maintainer to fix it. 
It uses plugin system to extend its functionality.

%package applet
Summary: CrashCatcher's applet
Group: User Interface/Desktops
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description applet
Simple systray applet to notify user about new events detected by crash-catcher 
daemon

%package gui
Summary: CrashCatcher's gui
Group: User Interface/Desktops
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: CrashCatcher's C/C++ addon
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and CrashCatcher's C/C++ 
language plugin.

%package plugin-sqlite3
Summary: CrashCatcher's SQLite3 database plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description plugin-sqlite3
This package contains SQLite3 database plugin. It is used for storing the data 
required for creating a bug report.

%package plugin-logger
Summary: CrashCatcher's logger reporter plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description plugin-logger
The simple reporter plugin, which writes a report to a specified file.

%package plugin-mailx
Summary: CrashCatcher's mailx reporter plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}
Requires: mailx

%description plugin-mailx
The simple reporter plugin, which sends a report via mailx to a specified email. 

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
rm -rf $RPM_BUILD_ROOT/%{_libdir}/lib*.la
rm -rf $RPM_BUILD_ROOT/%{_libdir}/crash-catcher/lib*.la
mkdir -p ${RPM_BUILD_ROOT}/etc/rc.d/init.d
install -m 755 $RPM_SOURCE_DIR/crash-catcher.init ${RPM_BUILD_ROOT}/etc/rc.d/init.d/crash-catcher
%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_bindir}/crash-catcher
%config(noreplace) %{_sysconfdir}/crash-catcher/crash-catcher.conf
%{_libdir}/lib*.so*
%{_sysconfdir}/dbus-1/system.d/dbus-crash-catcher.conf
%config /etc/rc.d/init.d/crash-catcher

%files applet
%{_bindir}/cc-applet

%files gui
%{_bindir}/cc-gui
%{_datadir}/crash-catcher/*.py*
%{_datadir}/crash-catcher/*.glade

%files addon-ccpp
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/CCpp.conf
%{_libdir}/crash-catcher/libCCpp.so*
%{_libexecdir}/hookCCpp

%files plugin-sqlite3
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/SQLite3.conf
%{_libdir}/crash-catcher/libSQLite3.so*

%files plugin-logger
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Logger.conf
%{_libdir}/crash-catcher/libLogger.so*

%files plugin-mailx
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Mailx.conf
%{_libdir}/crash-catcher/libMailx.so*

%changelog
* Tue Feb 24 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-2
- spec cleanup
- added new subpackage gui

* Wed Feb 18 2009 Zdenek Prikryl <zprikryl@redhat.com> 0.0.1-1
- initial packing
