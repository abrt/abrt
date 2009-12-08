%{!?python_site: %define python_site %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(0)")}
# platform-dependent
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
Summary: Automatic bug detection and reporting tool
Name: abrt
Version: 1.0.1
Release: 1%{?dist}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/abrt/
Source: http://jmoskovc.fedorapeople.org/%{name}-%{version}.tar.gz
Source1: abrt.init
BuildRequires: dbus-devel
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
BuildRequires: gettext
BuildRequires: nss-devel
BuildRequires: polkit-devel
BuildRequires: libzip-devel, libtar-devel, bzip2-devel, zlib-devel
BuildRequires: intltool
BuildRequires: bison
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: %{name}-libs = %{version}-%{release}
Requires(pre): shadow-utils
Obsoletes: abrt-plugin-sqlite3

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
Requires: dbus-python, pygtk2, pygtk2-libglade,
Requires: gnome-python2-gnomevfs, gnome-python2-gnomekeyring
# only if gtk2 version < 2.17:
#Requires: python-sexy
# we used to have abrt-applet, now abrt-gui includes it:
Provides: abrt-applet = %{version}-%{release}
Obsoletes: abrt-applet < 0.0.5
Conflicts: abrt-applet < 0.0.5

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: %{name}'s C/C++ addon
Group: System Environment/Libraries
Requires: gdb >= 7.0-3
Requires: elfutils
Requires: yum-utils
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and %{name}'s C/C++
analyzer plugin.

#%package plugin-firefox
#Summary: %{name}'s Firefox analyzer plugin
#Group: System Environment/Libraries
#Requires: gdb >= 7.0-3
#Requires: elfutils
#Requires: yum-utils
#Requires: %{name} = %{version}-%{release}

#%description plugin-firefox
#This package contains hook for Firefox

%package addon-kerneloops
Summary: %{name}'s kerneloops addon
Group: System Environment/Libraries
Requires: curl
Requires: %{name} = %{version}-%{release}
Obsoletes: kerneloops
Obsoletes: abrt-plugin-kerneloops
Obsoletes: abrt-plugin-kerneloopsreporter

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

%package plugin-catcut
Summary: %{name}'s catcut plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-catcut
Plugin to report bugs into the catcut.

%package plugin-ticketuploader
Summary: %{name}'s ticketuploader plugin
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description plugin-ticketuploader
Plugin to report bugs into anonymous FTP site associated with ticketing system.

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
# This package gets installed when anything requests bug-buddy -
# happens when users upgrade Fn to Fn+1;
# or if user just wants "typical desktop installation".
# Installing abrt-desktop should result in the abrt which works without
# any tweaking in abrt.conf (IOW: all plugins mentioned there must be installed)
Requires: %{name} = %{version}-%{release}
Requires: %{name}-addon-kerneloops
Requires: %{name}-addon-ccpp, %{name}-addon-python
Requires: %{name}-gui
Requires: %{name}-plugin-bugzilla, %{name}-plugin-logger, %{name}-plugin-runapp
#Requires: %{name}-plugin-firefox
Obsoletes: bug-buddy
Provides: bug-buddy

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
%find_lang %{name}

#rm -rf $RPM_BUILD_ROOT/%{_libdir}/lib*.la
#rm -rf $RPM_BUILD_ROOT/%{_libdir}/%{name}/lib*.la
# remove all .la and .a files
find $RPM_BUILD_ROOT -name '*.la' -or -name '*.a' | xargs rm -f
mkdir -p ${RPM_BUILD_ROOT}/%{_initrddir}
install -m 755 %SOURCE1 ${RPM_BUILD_ROOT}/%{_initrddir}/abrtd
mkdir -p $RPM_BUILD_ROOT/var/cache/%{name}
mkdir -p $RPM_BUILD_ROOT/var/cache/%{name}-di
mkdir -p $RPM_BUILD_ROOT/var/run/%{name}

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_datadir}/applications \
        --vendor fedora \
        --delete-original \
        ${RPM_BUILD_ROOT}%{_datadir}/applications/%{name}.desktop

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_sysconfdir}/xdg/autostart \
        src/Applet/%{name}-applet.desktop

%clean
rm -rf $RPM_BUILD_ROOT

%pre
getent group abrt >/dev/null || groupadd -f --system abrt
exit 0

%post
/sbin/chkconfig --add %{name}d

%post libs -p /sbin/ldconfig

%preun
if [ "$1" -eq "0" ] ; then
  service %{name}d stop >/dev/null 2>&1
  /sbin/chkconfig --del %{name}d
fi

%postun libs -p /sbin/ldconfig

%posttrans
if [ "$1" -eq "0" ]; then
    service %{name}d condrestart >/dev/null 2>&1 || :
fi

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc README COPYING
%{_sbindir}/%{name}d
%{_bindir}/%{name}-debuginfo-install
%{_bindir}/%{name}-backtrace
%config(noreplace) %{_sysconfdir}/%{name}/%{name}.conf
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-%{name}.conf
%{_initrddir}/%{name}d
%dir %attr(0775, root, abrt) %{_localstatedir}/cache/%{name}
%dir /var/run/%{name}
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/plugins
%dir %{_libdir}/%{name}
%{_mandir}/man8/abrtd.8.gz
%{_mandir}/man5/%{name}.conf.5.gz
#%{_mandir}/man5/pyhook.conf.5.gz
%{_mandir}/man7/%{name}-plugins.7.gz
%{_datadir}/polkit-1/actions/org.fedoraproject.abrt.policy
%{_datadir}/dbus-1/system-services/com.redhat.abrt.service
%config(noreplace) %{_sysconfdir}/%{name}/plugins/SQLite3.conf
%{_libdir}/%{name}/libSQLite3.so*
%{_mandir}/man7/%{name}-SQLite3.7.gz

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
%{_datadir}/applications/fedora-%{name}.desktop
%{_datadir}/pixmaps/abrt.png
%{_datadir}/icons/hicolor/48x48/apps/*.png
%{_bindir}/%{name}-applet
%{_sysconfdir}/xdg/autostart/%{name}-applet.desktop

%files addon-ccpp
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/CCpp.conf
%dir %{_localstatedir}/cache/%{name}-di
%{_libdir}/%{name}/libCCpp.so*
%{_libexecdir}/abrt-hook-ccpp

#%files plugin-firefox
#%{_libdir}/%{name}/libFirefox.so*

%files addon-kerneloops
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Kerneloops.conf
%{_bindir}/dumpoops
%{_libdir}/%{name}/libKerneloops.so*
%{_libdir}/%{name}/libKerneloopsScanner.so*
%{_mandir}/man7/%{name}-KerneloopsScanner.7.gz
%{_libdir}/%{name}/libKerneloopsReporter.so*
%{_libdir}/%{name}/KerneloopsReporter.GTKBuilder
%{_mandir}/man7/%{name}-KerneloopsReporter.7.gz

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

%files plugin-catcut
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/Catcut.conf
%{_libdir}/%{name}/libCatcut.so*
%{_libdir}/%{name}/Catcut.GTKBuilder
#%{_mandir}/man7/%{name}-Catcut.7.gz

%files plugin-ticketuploader
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/TicketUploader.conf
%{_libdir}/%{name}/libTicketUploader.so*
%{_libdir}/%{name}/TicketUploader.GTKBuilder
%{_mandir}/man7/%{name}-TicketUploader.7.gz

%files plugin-filetransfer
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}/plugins/FileTransfer.conf
%{_libdir}/%{name}/libFileTransfer.so*
%{_mandir}/man7/%{name}-FileTransfer.7.gz

%files addon-python
%defattr(-,root,root,-)
%attr(2755, root, abrt) %{_bindir}/%{name}-pyhook-helper
%{_libdir}/%{name}/libPython.so*
%{python_site}/*.py*

%files cli
%defattr(-,root,root,-)
%{_bindir}/abrt-cli
%{_mandir}/man1/abrt-cli.1.gz
%{_sysconfdir}/bash_completion.d/abrt-cli.bash

%files desktop
%defattr(-,root,root,-)

%changelog
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
