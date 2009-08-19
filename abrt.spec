%{!?python_site: %define python_site %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(0)")}
# platform-dependent
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
Summary: Automatic bug detection and reporting tool
Name: abrt
Version: 0.0.7.1
Release: 2%{?dist}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/abrt/
Source: http://jmoskovc.fedorapeople.org/%{name}-%{version}.tar.gz
Source1: abrt.init
BuildRequires: dbus-c++-devel
BuildRequires: gtk2-devel
BuildRequires: curl-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: sqlite-devel > 3.0
BuildRequires: desktop-file-utils
BuildRequires: nss-devel
BuildRequires: libnotify-devel
BuildRequires: xmlrpc-c-devel
BuildRequires: file-devel
BuildRequires: python-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: %{name}-libs = %{version}-%{release}

%description
%{name} is a tool to help users to detect defects in applications and
to create a bug report with all informations needed by maintainer to fix it.
It uses plugin system to extend its functionality.

%package libs
Summary: Libraries for %{name}
Group: System Environment/Libraries

%description libs
Libraries for %{name}.

%package devel
Summary: Development libraries for %{name}
Group: Development/Libraries
Requires: %{name}-libs = %{version}-%{release}

%description devel
Development libraries and headers for %{name}.

%package gui
Summary: %{name}'s gui
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}
Requires: dbus-python, pygtk2, pygtk2-libglade
Provides: abrt-applet = %{version}-%{release}
Obsoletes: abrt-applet < 0.0.5
Conflicts: abrt-applet < 0.0.5

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: %{name}'s C/C++ addon
Group: System Environment/Libraries
Requires: gdb
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and %{name}'s C/C++
analyzer plugin.

%package addon-kerneloops
Summary: %{name}'s kerneloops addon
Group: System Environment/Libraries
Requires: %{name}-plugin-kerneloopsreporter = %{version}-%{release}
Requires: %{name} = %{version}-%{release}
Conflicts: kerneloops
Obsoletes: abrt-plugin-kerneloops

%description addon-kerneloops
This package contains plugins for kernel crashes information collecting.

%package plugin-kerneloopsreporter
Summary: %{name}'s kerneloops reporter plugin
Group: System Environment/Libraries
Requires: curl
Requires: %{name} = %{version}-%{release}

%description plugin-kerneloopsreporter
This package contains reporter plugin, that sends, collected by %{name}'s kerneloops
addon, information about kernel crashes to specified server, e.g. kerneloops.org.

%package plugin-sqlite3
Summary: %{name}'s SQLite3 database plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-sqlite3
This package contains SQLite3 database plugin. It is used for storing the data
required for creating a bug report.

%package plugin-logger
Summary: %{name}'s logger reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-logger
The simple reporter plugin, which writes a report to a specified file.

%package plugin-mailx
Summary: %{name}'s mailx reporter plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}
Requires: mailx

%description plugin-mailx
The simple reporter plugin, which sends a report via mailx to a specified
email.

%package plugin-runapp
Summary: %{name}'s runapp plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-runapp
Plugin to run external programs.

%package plugin-sosreport
Summary: %{name}'s sosreport plugin
Group: System Environment/Libraries
Requires: sos
Requires: %{name} = %{version}-%{release}

%description plugin-sosreport
Plugin to include an sosreport in an abrt report.

%package plugin-bugzilla
Summary: %{name}'s bugzilla plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-bugzilla
Plugin to report bugs into the bugzilla.

%package plugin-filetransfer
Summary: %{name}'s File Transfer plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-filetransfer
Plugin to uploading files to a server.

%package addon-python
Summary: %{name}'s addon for catching and analyzing Python exceptions
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description addon-python
This package contains python hook and python analyzer plugin for hadnling
uncaught exception in python programs.

%package cli
Summary: %{name}'s command line interface
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}

%description cli
This package contains simple command line client for controling abrt daemon over
the sockets.

%package desktop
Summary: Virtual package to install all necessary packages for usage from desktop environment
Group: User Interface/Desktops
Requires: %{name} = %{version}-%{release}
Requires: %{name}-plugin-sqlite3, %{name}-plugin-bugzilla
Requires: %{name}-gui, %{name}-addon-kerneloops
Requires: %{name}-addon-ccpp, %{name}-addon-python

%description desktop
Virtual package to make easy default instalation on desktop environments.

%prep
%setup -q

%build
%configure
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT mandir=%{_mandir}

#rm -rf $RPM_BUILD_ROOT/%{_libdir}/lib*.la
#rm -rf $RPM_BUILD_ROOT/%{_libdir}/%{name}/lib*.la
# remove all .la and .a files
find $RPM_BUILD_ROOT -name '*.la' -or -name '*.a' | xargs rm -f
mkdir -p ${RPM_BUILD_ROOT}/%{_initrddir}
install -m 755 %SOURCE1 ${RPM_BUILD_ROOT}/%{_initrddir}/%{name}
mkdir -p $RPM_BUILD_ROOT/var/cache/%{name}

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_datadir}/applications \
        src/Gui/%{name}.desktop

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_sysconfdir}/xdg/autostart \
        src/Applet/%{name}-applet.desktop

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/chkconfig --add %{name}

%post libs -p /sbin/ldconfig

%preun
if [ "$1" = 0 ] ; then
  service %{name} stop >/dev/null 2>&1
  /sbin/chkconfig --del %{name}
fi

%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_sbindir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/%{name}.conf
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-%{name}.conf
%{_initrddir}/%{name}
%dir /var/cache/%{name}
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/plugins
%dir %{_libdir}/%{name}
%{_mandir}/man8/%{name}.8.gz
%{_mandir}/man5/%{name}.conf.5.gz
%{_mandir}/man7/%{name}-plugins.7.gz

%files libs
%defattr(-,root,root,-)
%{_libdir}/lib*.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/lib*.so

%files gui
%defattr(-,root,root,-)
%{_bindir}/%{name}-gui
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_bindir}/%{name}-applet
%{_sysconfdir}/xdg/autostart/%{name}-applet.desktop

%files addon-ccpp
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/CCpp.conf
%{_libdir}/%{name}/libCCpp.so*
%{_libexecdir}/hookCCpp

%files addon-kerneloops
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/KerneloopsScanner.conf
%{_bindir}/dumpoops
%{_libdir}/%{name}/libKerneloops.so*
%{_libdir}/%{name}/libKerneloopsScanner.so*
%{_mandir}/man7/%{name}-KerneloopsScanner.7.gz

%files plugin-kerneloopsreporter
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/KerneloopsReporter.conf
%{_libdir}/%{name}/libKerneloopsReporter.so*
%{_libdir}/%{name}/KerneloopsReporter.GTKBuilder
%{_mandir}/man7/%{name}-KerneloopsReporter.7.gz

%files plugin-sqlite3
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/SQLite3.conf
%{_libdir}/%{name}/libSQLite3.so*
%{_mandir}/man7/%{name}-SQLite3.7.gz

%files plugin-logger
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Logger.conf
%{_libdir}/%{name}/libLogger.so*
%{_libdir}/%{name}/Logger.GTKBuilder
%{_mandir}/man7/%{name}-Logger.7.gz

%files plugin-mailx
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Mailx.conf
%{_libdir}/%{name}/libMailx.so*
%{_libdir}/%{name}/Mailx.GTKBuilder
%{_mandir}/man7/%{name}-Mailx.7.gz

%files plugin-runapp
%defattr(-,root,root,-)
%{_libdir}/%{name}/libRunApp.so*
%{_mandir}/man7/%{name}-RunApp.7.gz

%files plugin-sosreport
%defattr(-,root,root,-)
%{_libdir}/%{name}/libSOSreport.so*

%files plugin-bugzilla
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Bugzilla.conf
%{_libdir}/%{name}/libBugzilla.so*
%{_libdir}/%{name}/Bugzilla.GTKBuilder
%{_mandir}/man7/%{name}-Bugzilla.7.gz

%files plugin-filetransfer
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/FileTransfer.conf
%{_libdir}/%{name}/libFileTransfer.so*
%{_mandir}/man7/%{name}-FileTransfer.7.gz

%files addon-python
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/pyhook.conf
%{python_sitearch}/ABRTUtils.so
%{_libdir}/%{name}/libPython.so*
%{python_site}/*.py*

%files cli
%defattr(-,root,root,-)
%{_bindir}/abrt-cli

%files desktop
%defattr(-,root,root,-)

%changelog
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
