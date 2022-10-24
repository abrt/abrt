# http://fedoraproject.org/wiki/Packaging:Guidelines#PIE
# http://fedoraproject.org/wiki/Hardened_Packages
%global _hardened_build 1

%if 0%{?fedora} || 0%{?rhel} > 7
# Enable python3 build by default
%bcond_without python3
%else
%bcond_with python3
%endif

%if 0%{?rhel}%{?suse_version}
    %bcond_with bodhi
%else
    %bcond_without bodhi
%endif

# build abrt-atomic subpackage
%bcond_without atomic

# build abrt-retrace-client by default
%bcond_without retrace

# rpmbuild --define 'desktopvendor mystring'
%if "x%{?desktopvendor}" == "x"
    %define desktopvendor %(source /etc/os-release; echo ${ID})
%endif

%if 0%{?suse_version}
%define dbus_devel dbus-1-devel
%define libjson_devel libjson-devel
%define shadow_utils pwdutils
%else
%define dbus_devel dbus-devel
%define libjson_devel json-c-devel
%define shadow_utils shadow-utils
%endif

# do not append package version to doc directory of subpackages in F20 and later; rhbz#993656
%if "%{_pkgdocdir}" == "%{_docdir}/%{name}"
    %define docdirversion %{nil}
%else
    %define docdirversion -%{version}
%endif

%define glib_ver 2.73.3
%define libreport_ver 2.14.0
%define satyr_ver 0.24

Summary: Automatic bug detection and reporting tool
Name: abrt
Version: 2.16.0
Release: 1
License: GPLv2+
URL: https://abrt.readthedocs.org/
Source: https://github.com/abrt/%{name}/archive/%{version}/%{name}-%{version}.tar.gz
BuildRequires: %{dbus_devel}
BuildRequires: hostname
BuildRequires: gtk3-devel
BuildRequires: glib2-devel >= %{glib_ver}
BuildRequires: rpm-devel >= 4.6
BuildRequires: desktop-file-utils
BuildRequires: libnotify-devel
#why? BuildRequires: file-devel
BuildRequires: make
BuildRequires: gettext
BuildRequires: libxml2-devel
BuildRequires: intltool
BuildRequires: libtool
BuildRequires: libsoup3-devel
BuildRequires: asciidoc
BuildRequires: doxygen
BuildRequires: xmlto
BuildRequires: libreport-devel >= %{libreport_ver}
BuildRequires: satyr-devel >= %{satyr_ver}
BuildRequires: augeas
BuildRequires: libselinux-devel
# Required for the %%{_unitdir} and %%{_tmpfilesdir} macros.
BuildRequires: systemd-rpm-macros
%if %{with python3}
BuildRequires: python3-devel
BuildRequires: python3-systemd
BuildRequires: python3-argcomplete
BuildRequires: python3-dbus

%if 0%{?fedora}
# https://docs.fedoraproject.org/en-US/packaging-guidelines/Python_Appendix/#_byte_compilation_reproducibility
%global py_reproducible_pyc_path %{buildroot}%{python3_sitelib}
BuildRequires: /usr/bin/marshalparser
%endif
%endif

Requires: libreport >= %{libreport_ver}
Requires: satyr >= %{satyr_ver}
# these only exist on suse
%if 0%{?suse_version}
BuildRequires: dbus-1-glib-devel
Requires: dbus-1-glib
%endif

%{?systemd_requires}
Requires: systemd
Requires: %{name}-libs = %{version}-%{release}
Requires: python3-%{name} = %{version}-%{release}
Requires(pre): %{shadow_utils}
%if %{with python3}
Requires: python3-augeas
Requires: python3-dbus
%endif
%ifarch aarch64 i686 x86_64
Requires: dmidecode
%endif
Requires: libreport-plugin-ureport
%if 0%{?fedora}
Requires: libreport-plugin-systemd-journal
%endif
# to fix upgrade path abrt-plugin-sosreport was removed in 2.14.5 version.
Obsoletes: abrt-plugin-sosreport < 2.14.5

#gui
BuildRequires: libreport-gtk-devel >= %{libreport_ver}
BuildRequires: gsettings-desktop-schemas-devel >= 3.15
#addon-ccpp
BuildRequires: gdb-headless
#addon-kerneloops
BuildRequires: systemd-devel
BuildRequires: %{libjson_devel}
%if %{with bodhi}
# plugin-bodhi
BuildRequires: libreport-web-devel >= %{libreport_ver}
%endif
#desktop
#Default config of addon-ccpp requires gdb
BuildRequires: gdb-headless
#dbus
BuildRequires: polkit-devel
%if %{with python3}
#python3-abrt
BuildRequires: python3-pytest
BuildRequires: python3-sphinx
BuildRequires: python3-libreport
#python3-abrt-doc
BuildRequires: python3-devel
%endif

%description
%{name} is a tool to help users to detect defects in applications and
to create a bug report with all information needed by maintainer to fix it.
It uses plugin system to extend its functionality.

%package libs
Summary: Libraries for %{name}

%description libs
Libraries for %{name}.

%package devel
Summary: Development libraries for %{name}
Requires: abrt-libs = %{version}-%{release}

%description devel
Development libraries and headers for %{name}.

%package gui-libs
Summary: Libraries for %{name}-gui
Requires: %{name}-libs = %{version}-%{release}

%description gui-libs
Libraries for %{name}-gui.

%package gui-devel
Summary: Development libraries for %{name}-gui
Requires: abrt-gui-libs = %{version}-%{release}

%description gui-devel
Development libraries and headers for %{name}-gui.

%package gui
Summary: %{name}'s gui
Requires: %{name} = %{version}-%{release}
Requires: %{name}-dbus = %{version}-%{release}
Requires: gnome-abrt
Requires: gsettings-desktop-schemas >= 3.15
# we used to have abrt-applet, now abrt-gui includes it:
Provides: abrt-applet = %{version}-%{release}
Obsoletes: abrt-applet < 0.0.5
Conflicts: abrt-applet < 0.0.5
Requires: abrt-libs = %{version}-%{release}
Requires: abrt-gui-libs = %{version}-%{release}

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: %{name}'s C/C++ addon
Requires: cpio
Requires: gdb-headless
Requires: elfutils
# Required for local retracing with GDB.
Requires: elfutils-debuginfod-client
%if 0%{!?rhel:1}
%endif
Requires: %{name} = %{version}-%{release}
Requires: abrt-libs = %{version}-%{release}
%if %{with python3}
Requires: python3-libreport
%endif
Obsoletes: abrt-addon-coredump-helper <= 2.12.2
Obsoletes: abrt-retrace-client <= 2.15.1


%description addon-ccpp
This package contains %{name}'s C/C++ analyzer plugin.

%package addon-upload-watch
Summary: %{name}'s upload addon
Requires: %{name} = %{version}-%{release}
Requires: abrt-libs = %{version}-%{release}

%description addon-upload-watch
This package contains hook for uploaded problems.

%package addon-kerneloops
Summary: %{name}'s kerneloops addon
Requires: curl
Requires: %{name} = %{version}-%{release}
%if 0%{!?rhel:1}
Requires: libreport-plugin-kerneloops >= %{libreport_ver}
%endif
Requires: abrt-libs = %{version}-%{release}

%description addon-kerneloops
This package contains plugin for collecting kernel crash information from
system log.

%package addon-xorg
Summary: %{name}'s Xorg addon
Requires: curl
Requires: %{name} = %{version}-%{release}
Requires: abrt-libs = %{version}-%{release}

%description addon-xorg
This package contains plugin for collecting Xorg crash information from Xorg
log.

%package addon-vmcore
Summary: %{name}'s vmcore addon
Requires: %{name} = %{version}-%{release}
Requires: abrt-addon-kerneloops
Requires: kexec-tools
%if %{with python3}
Requires: python3-abrt
Requires: python3-augeas
Requires: python3-systemd
%endif
Requires: util-linux

%description addon-vmcore
This package contains plugin for collecting kernel crash information from
vmcore files.

%package addon-pstoreoops
Summary: %{name}'s pstore oops addon
Requires: %{name} = %{version}-%{release}
Requires: abrt-libs = %{version}-%{release}
Requires: abrt-addon-kerneloops
Obsoletes: abrt-addon-uefioops <= 2.1.6
Provides: abrt-addon-uefioops = %{version}-%{release}

%description addon-pstoreoops
This package contains plugin for collecting kernel oopses from pstore storage.

%if %{with bodhi}
%package plugin-bodhi
Summary: %{name}'s bodhi plugin
Requires: %{name} = %{version}-%{release}
Obsoletes: libreport-plugin-bodhi <= 2.0.10
Provides: libreport-plugin-bodhi = %{version}-%{release}

%description plugin-bodhi
Search for a new updates in bodhi server.
%endif

%if %{with python3}
%package -n python3-abrt-addon
Summary: %{name}'s addon for catching and analyzing Python 3 exceptions
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: python3-systemd
Requires: python3-abrt

%description -n python3-abrt-addon
This package contains python 3 hook and python analyzer plugin for handling
uncaught exception in python 3 programs.

%package -n python3-abrt-container-addon
Summary: %{name}'s container addon for catching Python 3 exceptions
BuildArch: noarch
Conflicts: python3-abrt-addon
Requires: container-exception-logger

%description -n python3-abrt-container-addon
This package contains python 3 hook and handling uncaught exception in python 3
programs in container.
%endif

%package plugin-machine-id
Summary: %{name}'s plugin to generate machine_id based off dmidecode
Requires: %{name} = %{version}-%{release}

%description plugin-machine-id
This package contains a configuration snippet to enable automatic generation
of machine_id for abrt events.

%package tui
Summary: %{name}'s command line interface
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: libreport-cli >= %{libreport_ver}
Requires: abrt-libs = %{version}-%{release}
Requires: abrt-dbus
%if %{with python3}
Requires: python3-abrt
Requires: abrt-addon-ccpp
Requires: python3-argcomplete

Provides: %{name}-cli-ng = %{version}-%{release}
Obsoletes: %{name}-cli-ng < 2.12.2
%endif

%description tui
This package contains a simple command line client for processing abrt reports
in command line environment.

%package cli
Summary: Virtual package to make easy default installation on non-graphical environments
Requires: %{name} = %{version}-%{release}
Requires: abrt-tui
Requires: abrt-addon-kerneloops
Requires: abrt-addon-pstoreoops
Requires: abrt-addon-vmcore
Requires: abrt-addon-ccpp
%if %{with python3}
Requires: python3-abrt-addon
%endif
Requires: abrt-addon-xorg
%if ! 0%{?rhel}
%if %{with bodhi}
Requires: abrt-plugin-bodhi
%endif
%if 0%{!?suse_version:1}
Requires: libreport-plugin-bugzilla >= %{libreport_ver}
%endif
Requires: libreport-plugin-logger >= %{libreport_ver}
Requires: libreport-plugin-ureport >= %{libreport_ver}
%if 0%{?fedora}
Requires: libreport-fedora >= %{libreport_ver}
%endif
%endif

%description cli
Virtual package to install all necessary packages for usage from command line
environment.

%package desktop
Summary: Virtual package to make easy default installation on desktop environments
# This package gets installed when anything requests bug-buddy -
# happens when users upgrade Fn to Fn+1;
# or if user just wants "typical desktop installation".
# Installing abrt-desktop should result in the abrt which works without
# any tweaking in abrt.conf (IOW: all plugins mentioned there must be installed)
Requires: %{name} = %{version}-%{release}
Requires: abrt-addon-kerneloops
Requires: abrt-addon-pstoreoops
Requires: abrt-addon-vmcore
Requires: abrt-addon-ccpp
%if %{with python3}
Requires: python3-abrt-addon
%endif
Requires: abrt-addon-xorg
Requires: gdb-headless
Requires: abrt-gui
Requires: gnome-abrt
%if ! 0%{?rhel}
%if %{with bodhi}
Requires: abrt-plugin-bodhi
%endif
%if 0%{!?suse_version:1}
Requires: libreport-plugin-bugzilla >= %{libreport_ver}
%endif
Requires: libreport-plugin-logger >= %{libreport_ver}
Requires: libreport-plugin-ureport >= %{libreport_ver}
%if 0%{?fedora}
Requires: libreport-fedora >= %{libreport_ver}
%endif
%endif
#Requires: abrt-plugin-firefox
Provides: bug-buddy = %{version}-%{release}

%description desktop
Virtual package to install all necessary packages for usage from desktop
environment.

%if %{with atomic}
%package atomic
Summary: Package to make easy default installation on Atomic hosts.
Requires: %{name}-libs = %{version}-%{release}
Conflicts: %{name}-addon-ccpp

%description atomic
Package to install all necessary packages for usage from Atomic
hosts.
%endif

%package dbus
Summary: ABRT DBus service
Requires: %{name} = %{version}-%{release}
Requires: abrt-libs = %{version}-%{release}
Requires: dbus-tools

%description dbus
ABRT DBus service which provides org.freedesktop.problems API on dbus and
uses PolicyKit to authorize to access the problem data.

%if %{with python3}
%package -n python3-abrt
Summary: ABRT Python 3 API
Requires: %{name} = %{version}-%{release}
Requires: %{name}-libs = %{version}-%{release}
Requires: %{name}-dbus = %{version}-%{release}
Requires: python3-dbus
Requires: python3-libreport
Requires: python3-gobject-base

%description -n python3-abrt
High-level API for querying, creating and manipulating
problems handled by ABRT in Python 3.

%package -n python3-abrt-doc
Summary: ABRT Python API Documentation
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: python3-%{name} = %{version}-%{release}

%description -n python3-abrt-doc
Examples and documentation for ABRT Python 3 API.
%endif

%package console-notification
Summary: ABRT console notification script
Requires: %{name} = %{version}-%{release}
Requires: %{name}-cli = %{version}-%{release}

%description console-notification
A small script which prints a count of detected problems when someone logs in
to the shell

%prep
%setup -q

%build
./autogen.sh

%define default_dump_dir %{_localstatedir}/spool/abrt

CFLAGS="%{optflags} -Werror" %configure \
%if %{without python3}
        --without-python3 \
%endif
%if %{without bodhi}
        --without-bodhi \
%endif
%if %{without atomic}
        --without-atomic \
%endif
%ifnarch %{arm}
        --enable-native-unwinder \
%endif
        --with-defaultdumplocation=%{default_dump_dir} \
        --enable-doxygen-docs \
        --enable-dump-time-unwind \
        --disable-silent-rules

%make_build

%install
%make_install \
%if %{with python3}
             PYTHON=%{__python3} \
%endif
             dbusabrtdocdir=%{_defaultdocdir}/%{name}-dbus%{docdirversion}/html/

%find_lang %{name}

# Remove byte-compiled python files generated by automake.
# automake uses system's python for all *.py files, even
# for those which needs to be byte-compiled with different
# version (python2/python3).
# rpm can do this work and use the appropriate python version.
find %{buildroot} -name "*.py[co]" -delete

# remove all .la and .a files
find %{buildroot} -name '*.la' -or -name '*.a' | xargs rm -f
mkdir -p %{buildroot}%{_localstatedir}/cache/abrt-di
mkdir -p %{buildroot}%{_localstatedir}/lib/abrt
mkdir -p %{buildroot}%{_localstatedir}/run/abrt
mkdir -p %{buildroot}%{_localstatedir}/spool/abrt-upload
mkdir -p %{buildroot}%{default_dump_dir}

desktop-file-install \
        --dir %{buildroot}%{_datadir}/applications \
        src/applet/org.freedesktop.problems.applet.desktop

ln -sf %{_datadir}/applications/org.freedesktop.problems.applet.desktop %{buildroot}%{_sysconfdir}/xdg/autostart/
%if %{with python3}
ln -sf %{_bindir}/abrt %{buildroot}%{_bindir}/abrt-cli
ln -sf %{_mandir}/man1/abrt.1 %{buildroot}%{_mandir}/man1/abrt-cli.1
%endif

# After everything is installed, remove info dir
rm -f %{buildroot}%{_infodir}/dir

%check
make check|| {
    # find and print the logs of failed test
    # do not cat tests/testsuite.log because it contains a lot of bloat
    find src -name "test-suite.log" -print -exec cat '{}' \;
    find tests/testsuite.dir -name "testsuite.log" -print -exec cat '{}' \;
    exit 1
}

%pre
#uidgid pair 173:173 reserved in setup rhbz#670231
%define abrt_gid_uid 173
getent group abrt >/dev/null || groupadd -f -g %{abrt_gid_uid} --system abrt
getent passwd abrt >/dev/null || useradd --system -g abrt -u %{abrt_gid_uid} -d /etc/abrt -s /sbin/nologin abrt
exit 0

%post
# $1 == 1 if install; 2 if upgrade
%systemd_post abrtd.service

%post addon-ccpp
# migration from 2.14.1.18
if [ ! -e "%{_localstatedir}/cache/abrt-di/.migration-group-add" ]; then
  chmod -R g+w %{_localstatedir}/cache/abrt-di
  touch "%{_localstatedir}/cache/abrt-di/.migration-group-add"
fi

%systemd_post abrt-journal-core.service
%journal_catalog_update

%post addon-kerneloops
%systemd_post abrt-oops.service
%journal_catalog_update

%post addon-xorg
%systemd_post abrt-xorg.service
%journal_catalog_update

%if %{with python3}
%post -n python3-abrt-addon
%journal_catalog_update
%endif

%post addon-vmcore
%systemd_post abrt-vmcore.service
%journal_catalog_update

%post addon-pstoreoops
%systemd_post abrt-pstoreoops.service

%post addon-upload-watch
%systemd_post abrt-upload-watch.service

%preun
%systemd_preun abrtd.service

%preun addon-ccpp
%systemd_preun abrt-journal-core.service

%preun addon-kerneloops
%systemd_preun abrt-oops.service

%preun addon-xorg
%systemd_preun abrt-xorg.service

%preun addon-vmcore
%systemd_preun abrt-vmcore.service

%preun addon-pstoreoops
%systemd_preun abrt-pstoreoops.service

%preun addon-upload-watch
%systemd_preun abrt-upload-watch.service

%postun
%systemd_postun_with_restart abrtd.service

%postun addon-ccpp
%systemd_postun_with_restart abrt-journal-core.service

%postun addon-kerneloops
%systemd_postun_with_restart abrt-oops.service

%postun addon-xorg
%systemd_postun_with_restart abrt-xorg.service

%postun addon-vmcore
%systemd_postun_with_restart abrt-vmcore.service

%postun addon-pstoreoops
%systemd_postun_with_restart abrt-pstoreoops.service

%postun addon-upload-watch
%systemd_postun_with_restart abrt-upload-watch.service

%post gui
# update icon cache
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%if %{with atomic}
%post atomic
if [ -f /etc/abrt/plugins/CCpp.conf ]; then
    mv /etc/abrt/plugins/CCpp.conf /etc/abrt/plugins/CCpp.conf.rpmsave.atomic || exit 1;
fi

%preun atomic
if [ -L /etc/abrt/plugins/CCpp.conf ]; then
    rm /etc/abrt/plugins/CCpp.conf
fi
if [ -f /etc/abrt/plugins/CCpp.conf.rpmsave.atomic ]; then
    mv /etc/abrt/plugins/CCpp.conf.rpmsave.atomic /etc/abrt/plugins/CCpp.conf || exit 1
fi
%endif

%if 0%{?rhel} && 0%{?rhel} <= 7
%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%post gui-libs -p /sbin/ldconfig

%postun gui-libs -p /sbin/ldconfig

%postun gui
if [ $1 -eq 0 ] ; then
    touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi
%endif

%posttrans
# update the old problem dirs to contain "type" element
service abrtd condrestart >/dev/null 2>&1 || :

%posttrans addon-ccpp
# Regenerate core_backtraces because of missing crash threads
abrtdir=$(grep "^\s*DumpLocation\b" /etc/abrt/abrt.conf | tail -1 | cut -d'=' -f2 | tr -d ' ')
if test -z "$abrtdir"; then
    abrtdir=%{default_dump_dir}
fi
if test -d "$abrtdir"; then
    for DD in `find "$abrtdir" -mindepth 1 -maxdepth 1 -type d`
    do
        if test -f "$DD/analyzer" && grep -q "^CCpp$" "$DD/analyzer"; then
            /usr/bin/abrt-action-generate-core-backtrace -d "$DD" -- >/dev/null 2>&1 || :
            test -f "$DD/core_backtrace" && chown `stat --format=%U:abrt $DD` "$DD/core_backtrace" || :
        fi
    done
fi

%posttrans addon-kerneloops
service abrt-oops condrestart >/dev/null 2>&1 || :

%posttrans addon-xorg
service abrt-xorg condrestart >/dev/null 2>&1 || :

%posttrans addon-vmcore
service abrt-vmcore condrestart >/dev/null 2>&1 || :
# Copy the configuration file to plugin's directory
test -f /etc/abrt/abrt-harvest-vmcore.conf && {
    echo "Moving /etc/abrt/abrt-harvest-vmcore.conf to /etc/abrt/plugins/vmcore.conf"
    mv -b /etc/abrt/abrt-harvest-vmcore.conf /etc/abrt/plugins/vmcore.conf
}
exit 0

%posttrans addon-pstoreoops
service abrt-pstoreoops condrestart >/dev/null 2>&1 || :

%posttrans dbus
# Force abrt-dbus to restart like we do with the other services
killall abrt-dbus >/dev/null 2>&1 || :

%files -f %{name}.lang
%doc README.md COPYING
%{_unitdir}/abrtd.service
%{_tmpfilesdir}/abrt.conf
%{_sbindir}/abrtd
%{_sbindir}/abrt-server
%{_sbindir}/abrt-auto-reporting
%{_libexecdir}/abrt-handle-event
%{_libexecdir}/abrt-action-ureport
%{_libexecdir}/abrt-action-save-container-data
%{_bindir}/abrt-handle-upload
%{_bindir}/abrt-action-notify
%{_mandir}/man1/abrt-action-notify.1*
%{_bindir}/abrt-action-save-package-data
%{_bindir}/abrt-watch-log
%{_bindir}/abrt-action-analyze-python
%{_bindir}/abrt-action-analyze-xorg
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.problems.daemon.conf
%config(noreplace) %{_sysconfdir}/%{name}/abrt.conf
%config(noreplace) %{_sysconfdir}/%{name}/abrt-action-save-package-data.conf
%config(noreplace) %{_sysconfdir}/%{name}/gpg_keys.conf
%config(noreplace) %{_sysconfdir}/libreport/events.d/abrt_event.conf
%{_mandir}/man5/abrt_event.conf.5*
%config(noreplace) %{_sysconfdir}/libreport/events.d/smart_event.conf
%{_mandir}/man5/smart_event.conf.5*
%dir %attr(0751, root, abrt) %{default_dump_dir}
%dir %attr(0700, abrt, abrt) %{_localstatedir}/spool/%{name}-upload
# abrtd runs as root
%ghost %dir %attr(0755, root, root) %{_localstatedir}/run/%{name}
%ghost %attr(0666, -, -) %{_localstatedir}/run/%{name}/abrt.socket
%ghost %attr(0644, -, -) %{_localstatedir}/run/%{name}/abrtd.pid

%{_mandir}/man1/abrt-handle-upload.1*
%{_mandir}/man1/abrt-server.1*
%{_mandir}/man1/abrt-action-save-package-data.1*
%{_mandir}/man1/abrt-watch-log.1*
%{_mandir}/man1/abrt-action-analyze-python.1*
%{_mandir}/man1/abrt-action-analyze-xorg.1*
%{_mandir}/man1/abrt-auto-reporting.1*
%{_mandir}/man5/abrt.conf.5*
%{_mandir}/man5/abrt-action-save-package-data.conf.5*
%{_mandir}/man5/gpg_keys.conf.5*
%{_mandir}/man8/abrtd.8*

%files libs
%{_libdir}/libabrt.so.*
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/plugins
%dir %{_datadir}/%{name}

# filesystem package should own /usr/share/augeas/lenses directory
%{_datadir}/augeas/lenses/abrt.aug

%files devel
# The complex pattern below (instead of simlpy *) excludes Makefile{.am,.in}:
%doc apidoc/html/*.{html,png,css,js}
%{_includedir}/abrt/abrt-dbus.h
%{_includedir}/abrt/hooklib.h
%{_includedir}/abrt/libabrt.h
%{_includedir}/abrt/problem_api.h
%{_libdir}/libabrt.so
%{_libdir}/pkgconfig/abrt.pc

%files gui-libs
%{_libdir}/libabrt_gui.so.*

%files gui-devel
%{_includedir}/abrt/abrt-config-widget.h
%{_includedir}/abrt/system-config-abrt.h
%{_libdir}/libabrt_gui.so
%{_libdir}/pkgconfig/abrt_gui.pc

%files gui
%dir %{_datadir}/%{name}
# all glade, gtkbuilder and py files for gui
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/%{name}/ui/*
%{_bindir}/abrt-applet
%{_bindir}/system-config-abrt
#%%{_bindir}/test-report
%{_datadir}/applications/org.freedesktop.problems.applet.desktop
%config(noreplace) %{_sysconfdir}/xdg/autostart/org.freedesktop.problems.applet.desktop
%{_datadir}/dbus-1/services/org.freedesktop.problems.applet.service
%{_mandir}/man1/abrt-applet.1*
%{_mandir}/man1/system-config-abrt.1*

%files addon-ccpp
%dir %attr(0775, abrt, abrt) %{_localstatedir}/cache/abrt-di
%config(noreplace) %{_sysconfdir}/%{name}/plugins/CCpp.conf
%{_mandir}/man5/abrt-CCpp.conf.5*
%{_libexecdir}/abrt-gdb-exploitable
%config(noreplace) %{_sysconfdir}/libreport/plugins/catalog_journal_ccpp_format.conf
%{_unitdir}/abrt-journal-core.service
%{_journalcatalogdir}/abrt_ccpp.catalog

%dir %{_localstatedir}/lib/abrt

%{_bindir}/abrt-action-analyze-c
%{_bindir}/abrt-action-trim-files
%{_bindir}/abrt-action-analyze-vulnerability
%{_bindir}/abrt-action-generate-backtrace
%{_bindir}/abrt-action-generate-core-backtrace
%{_bindir}/abrt-action-analyze-backtrace
%{_bindir}/abrt-action-list-dsos
%{_bindir}/abrt-action-analyze-ccpp-local
%{_bindir}/abrt-dump-journal-core
%config(noreplace) %{_sysconfdir}/libreport/events.d/ccpp_event.conf
%{_mandir}/man5/ccpp_event.conf.5*
%config(noreplace) %{_sysconfdir}/libreport/events.d/gconf_event.conf
%{_mandir}/man5/gconf_event.conf.5*
%config(noreplace) %{_sysconfdir}/libreport/events.d/vimrc_event.conf
%{_mandir}/man5/vimrc_event.conf.5*
%{_datadir}/libreport/events/analyze_CCpp.xml
%{_datadir}/libreport/events/analyze_LocalGDB.xml
%{_datadir}/libreport/events/collect_xsession_errors.xml
%{_datadir}/libreport/events/collect_GConf.xml
%{_datadir}/libreport/events/collect_vimrc_user.xml
%{_datadir}/libreport/events/collect_vimrc_system.xml
%{_datadir}/libreport/events/post_report.xml
%{_mandir}/man*/abrt-action-analyze-c.*
%{_mandir}/man*/abrt-action-trim-files.*
%{_mandir}/man*/abrt-action-generate-backtrace.*
%{_mandir}/man*/abrt-action-generate-core-backtrace.*
%{_mandir}/man*/abrt-action-analyze-backtrace.*
%{_mandir}/man*/abrt-action-list-dsos.*
%{_mandir}/man*/abrt-action-analyze-ccpp-local.*
%{_mandir}/man*/abrt-action-analyze-vulnerability.*
%{_mandir}/man1/abrt-dump-journal-core.1*

%files addon-upload-watch
%{_sbindir}/abrt-upload-watch
%{_unitdir}/abrt-upload-watch.service
%{_mandir}/man*/abrt-upload-watch.*


%files addon-kerneloops
%config(noreplace) %{_sysconfdir}/libreport/events.d/koops_event.conf
%{_journalcatalogdir}/abrt_koops.catalog
%config(noreplace) %{_sysconfdir}/libreport/plugins/catalog_koops_format.conf
%{_mandir}/man5/koops_event.conf.5*
%config(noreplace) %{_sysconfdir}/%{name}/plugins/oops.conf
%{_unitdir}/abrt-oops.service

%dir %{_localstatedir}/lib/abrt

%{_bindir}/abrt-dump-oops
%{_bindir}/abrt-dump-journal-oops
%{_bindir}/abrt-action-analyze-oops
%{_mandir}/man1/abrt-dump-oops.1*
%{_mandir}/man1/abrt-dump-journal-oops.1*
%{_mandir}/man1/abrt-action-analyze-oops.1*
%{_mandir}/man5/abrt-oops.conf.5*

%files addon-xorg
%config(noreplace) %{_sysconfdir}/libreport/events.d/xorg_event.conf
%{_journalcatalogdir}/abrt_xorg.catalog
%config(noreplace) %{_sysconfdir}/libreport/plugins/catalog_xorg_format.conf
%config(noreplace) %{_sysconfdir}/%{name}/plugins/xorg.conf
%{_unitdir}/abrt-xorg.service
%{_bindir}/abrt-dump-xorg
%{_bindir}/abrt-dump-journal-xorg
%{_mandir}/man1/abrt-dump-xorg.1*
%{_mandir}/man1/abrt-dump-journal-xorg.1*
%{_mandir}/man5/abrt-xorg.conf.5*
%{_mandir}/man5/xorg_event.conf.5*

%files addon-vmcore
%config(noreplace) %{_sysconfdir}/libreport/events.d/vmcore_event.conf
%{_mandir}/man5/vmcore_event.conf.5*
%config(noreplace) %{_sysconfdir}/%{name}/plugins/vmcore.conf
%{_datadir}/libreport/events/analyze_VMcore.xml
%{_unitdir}/abrt-vmcore.service
%{_sbindir}/abrt-harvest-vmcore
%{_bindir}/abrt-action-analyze-vmcore
%{_bindir}/abrt-action-check-oops-for-alt-component
%{_bindir}/abrt-action-check-oops-for-hw-error
%{_mandir}/man1/abrt-harvest-vmcore.1*
%{_mandir}/man5/abrt-vmcore.conf.5*
%{_mandir}/man1/abrt-action-analyze-vmcore.1*
%{_mandir}/man1/abrt-action-check-oops-for-hw-error.1*
%{_journalcatalogdir}/abrt_vmcore.catalog
%config(noreplace) %{_sysconfdir}/libreport/plugins/catalog_vmcore_format.conf

%files addon-pstoreoops
%{_unitdir}/abrt-pstoreoops.service
%{_sbindir}/abrt-harvest-pstoreoops
%{_bindir}/abrt-merge-pstoreoops
%{_mandir}/man1/abrt-harvest-pstoreoops.1*
%{_mandir}/man1/abrt-merge-pstoreoops.1*

%if %{with python3}
%files -n python3-abrt-addon
%config(noreplace) %{_sysconfdir}/%{name}/plugins/python3.conf
%{_mandir}/man5/python3-abrt.conf.5*
%config(noreplace) %{_sysconfdir}/libreport/events.d/python3_event.conf
%{_journalcatalogdir}/python3_abrt.catalog
%config(noreplace) %{_sysconfdir}/libreport/plugins/catalog_python3_format.conf
%{_mandir}/man5/python3_event.conf.5*
%{python3_sitelib}/abrt3.pth
%{python3_sitelib}/abrt_exception_handler3.py
%{python3_sitelib}/__pycache__/abrt_exception_handler3.*

%files -n python3-abrt-container-addon
%{python3_sitelib}/abrt3_container.pth
%{python3_sitelib}/abrt_exception_handler3_container.py
%{python3_sitelib}/__pycache__/abrt_exception_handler3_container.*
%endif

%files plugin-machine-id
%config(noreplace) %{_sysconfdir}/libreport/events.d/machine-id_event.conf
%{_libexecdir}/abrt-action-generate-machine-id

%files cli

%files tui
%if %{with python3}
%{_bindir}/abrt
%{_bindir}/abrt-cli
%{python3_sitelib}/abrtcli/
%{_mandir}/man1/abrt.1*
%{_mandir}/man1/abrt-cli.1*
%endif

%files desktop

%if %{with atomic}
%files atomic
%config(noreplace) %{_sysconfdir}/%{name}/abrt-action-save-package-data.conf
%{_bindir}/abrt-action-save-package-data
%{_mandir}/man1/abrt-action-save-package-data.1*
%{_mandir}/man5/abrt-action-save-package-data.conf.5*
%endif

%if %{with bodhi}
%files plugin-bodhi
%{_bindir}/abrt-bodhi
%{_bindir}/abrt-action-find-bodhi-update
%config(noreplace) %{_sysconfdir}/libreport/events.d/bodhi_event.conf
%{_datadir}/libreport/events/analyze_BodhiUpdates.xml
%{_mandir}/man1/abrt-bodhi.1*
%{_mandir}/man1/abrt-action-find-bodhi-update.1*
%endif

%files dbus
%{_sbindir}/abrt-dbus
%{_mandir}/man8/abrt-dbus.8*
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-abrt.conf
%{_datadir}/dbus-1/interfaces/org.freedesktop.Problems2.xml
%{_datadir}/dbus-1/interfaces/org.freedesktop.Problems2.Entry.xml
%{_datadir}/dbus-1/interfaces/org.freedesktop.Problems2.Session.xml
%{_datadir}/dbus-1/interfaces/org.freedesktop.Problems2.Task.xml
%{_datadir}/dbus-1/system-services/org.freedesktop.problems.service
%{_datadir}/polkit-1/actions/abrt_polkit.policy
%dir %{_defaultdocdir}/%{name}-dbus%{docdirversion}/
%dir %{_defaultdocdir}/%{name}-dbus%{docdirversion}/html/
%{_defaultdocdir}/%{name}-dbus%{docdirversion}/html/*.html
%{_defaultdocdir}/%{name}-dbus%{docdirversion}/html/*.css
%config(noreplace) %{_sysconfdir}/libreport/events.d/abrt_dbus_event.conf

%if %{with python3}
%files -n python3-abrt
%{python3_sitearch}/problem/
%{_mandir}/man5/python3-abrt.5*

%files -n python3-abrt-doc
%{python3_sitelib}/problem_examples
%endif

%files console-notification
%config(noreplace) %{_sysconfdir}/profile.d/abrt-console-notification.sh

%changelog
* Mon Oct 24 2022 Michal Srb <michal@redhat.com> 2.16.0-1
- Update changelog (michal@redhat.com)
- a-a-gen-bt: Increase timeout to 20 minutes (michal@redhat.com)
- Keep abrt_get_backtrace() backward compatible. (michal@redhat.com)
- a-dump-journal-core: First seek the journal tail and then set filters
  (michal@redhat.com)
- ci: Pull the latest fedora from quay.io (michal@redhat.com)
- abrt-journal: call sd_journal_get_fd() right after sd_journal_open()
  (michal@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- lib: Bump library version (mgrabovs@redhat.com)
- spec: Obsolete abrt-retrace-client due to removal (mgrabovs@redhat.com)
- spec: Require elfutils-debuginfod-client (mgrabovs@redhat.com)
- spec: Require systemd-rpm-macros in build (mgrabovs@redhat.com)
- applet: Update GLib constant name (mgrabovs@redhat.com)
- a-a-gen-bt: Increase default timeout to 15 minutes (mgrabovs@redhat.com)
- Remove remote retracing capabilities (mgrabovs@redhat.com)
- Generate backtraces locally using debuginfod integration
  (mgrabovs@redhat.com)
- Remove old test (mgrabovs@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- cli: use python3-argcomplete global completion (frederic@moulins.org)
- lib: Use time_t for timestamps (mgrabovs@redhat.com)
- retrace-client: Port to libsoup3 (mcrha@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- spec: Fix macro expansion (mfabik@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- Fix for rpm 4.18 (michal@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- a-dump-journal-oops: Check return value of abrt_journal_seek_tail()
  (mgrabovs@redhat.com)
- abrt-oops: Follow journal from the end if the position cannot be restored
  (michal@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- check_recent_crash_file: Check return value of lseek() (mgrabovs@redhat.com)
- [man] Update man page for abrt-bodhi (michal@redhat.com)

* Thu Mar 10 2022 Michal Srb <michal@redhat.com> 2.15.1-1
- Update changelog (michal@redhat.com)
- Revert "Don't copy coredump to problem dir" (michal@redhat.com)
- Revert "Add abrt_save_coredump function" (michal@redhat.com)
- Revert "a-a-generate-core-backtrace: Save coredump" (michal@redhat.com)
- Revert "a-a-generate-backtrace: Save coredump" (michal@redhat.com)
- a-retrace-client: Check return value; code style (mgrabovs@redhat.com)
- a-watch-log: Check return value of lseek (mgrabovs@redhat.com)
- spec: Condition marshalparser on %%fedora (mgrabovs@redhat.com)
- applet,daemon: Check return values (mgrabovs@redhat.com)
- lib: Check return values (mgrabovs@redhat.com)
- plugins: Check return values (mgrabovs@redhat.com)
- a-auto-reporting: Remove dead code (mgrabovs@redhat.com)
- bodhi: Check string for non-emptiness (mgrabovs@redhat.com)
- abrt_conf: Remove dead code (mgrabovs@redhat.com)
- a-a-save-package-data: Free data before rewriting it (mgrabovs@redhat.com)
- abrt-server: Free message buffer automatically (mgrabovs@redhat.com)
- a-retrace-client: Free strings manually (mgrabovs@redhat.com)
- Fix issues with conflicting multilib packages (miro@hroncok.cz)
- abrt-dump-journal: Don't die on corrupt data (mfabik@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- tito: Improve custom tagger (mgrabovs@redhat.com)

* Tue Feb 22 2022 Miro Hrončok <mhroncok@redhat.com> - 2.15.0-2
- Make abrt-tui and python3-abrt-container-addon noarch as they contain no architecture-specific content
- Ensure Python bytecode in noarch subpackages is reproducible

* Mon Jan 17 2022 Matěj Grabovský <mgrabovs@redhat.com> 2.15.0-1
- tito: Fix custom tagger (mgrabovs@redhat.com)
- lib: Bump library version to 1:0:1 (mgrabovs@redhat.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- Update translations (mgrabovsky@users.noreply.github.com)
- tests: Fix calls to deprecated methods (mgrabovs@redhat.com)
- Update translations (mgrabovs@redhat.com)
- Update abrt-journal.c (48353898+copperii@users.noreply.github.com)
- cli: Fix path and glob matching for abrt info etc. (mfabik@redhat.com)
- abrt-dump-oops: Fix vmcore call trace parsing (mfabik@redhat.com)
- tito: Custom tagger for updating changelog (mgrabovs@redhat.com)
- abrt-dbus: do not try to free session data twice (michal@redhat.com)
- changelog: Move entry to correct release (mgrabovs@redhat.com)
- Use lazy imports in abrt_exception_handler3: changelog entry
  (miro@hroncok.cz)
- Use lazy imports in abrt_exception_handler3 (miro@hroncok.cz)
- Update changelog (mgrabovs@redhat.com)
- a-a-generate-backtrace: Save coredump (mfabik@redhat.com)
- a-a-generate-core-backtrace: Save coredump (mfabik@redhat.com)
- Add abrt_save_coredump function (mfabik@redhat.com)
- Don't copy coredump to problem dir (mfabik@redhat.com)
- Prevent unnecessary duplication of test metadata (psplicha@redhat.com)
- Make it possible to run a subset of integration tests via TMT
  (michal@redhat.com)
- a-a-save-package-data: Update regex for Perl interpreter
  (mgrabovs@redhat.com)
- a-a-save-package-data: Update regex for Python 3.10 (mgrabovs@redhat.com)
- readme: Add badge for build status (mgrabovs@redhat.com)
- readme: Update IRC channel (mgrabovs@redhat.com)
- a-a-find-bodhi-update: Fix grammar and spelling (mgrabovs@redhat.com)
- a-a-find-bodhi-update: More refactoring (mgrabovs@redhat.com)
- a-a-find-bodhi-update: Refactoring (mgrabovs@redhat.com)

* Mon May 24 2021 Michal Fabik <mfabik@redhat.com> 2.14.6-1
- new package built with tito
* Sat May 01 2021 Sérgio Basto <sergio@serjux.com> - 2.14.5-2
- Obsoletes abrt-plugin-sosreport

* Tue Dec 01 2020 Michal Fabik <mfabik@redhat.com> 2.14.5-1
- BR make
- Update translations
- spec: Don't use GitHub-generated tarball as Source
- changelog: Add fix for rhbz#1900982
- rpm: Correct NEVRA format
- tests: Fix leaks
- tests: Only fail on definite and indirect leaks
- CONTRIBUTING: Add changelog instructions
- CHANGELOG: Update with PR 1537
- abrtd: Don't jump over g_autofree
- hooklib: Proper freeing of backtrace
- hooklib: Don't g_autofree backtrace
- abrt-action-generate-core-backtrace: Drop --raw flag
- a-a-install-debuginfo: Correct debuginfo numbers
- spec: Output logs on make check failure
- spec: Bump libreport_ver, require python3-dbus
- tests: upload-handling: Quote paths more liberally
- Drop sosreport_event.conf
- spec: Fix Obsoletes

* Mon Aug 17 2020 Michal Fabik <mfabik@redhat.com> 2.14.4-1
- oops-utils: Respect the 'world-readable' flag
- Decommission libreport_list_free_with_free
- Purge commented-out code

* Wed Aug 12 2020 Michal Fabik <mfabik@redhat.com> 2.14.3-1
- Update translations
- plugins: abrt-dump-journal-core: Handle zstd compression
- lib: Check rename() return code
- retrace-client: Fix memory leak
- a-a-save-container-data: Fix possible NULL dereferences
- lib: Fix resource leak
- lib: Remove ignored_problems
- lib,a-retrace-client: Check fstat() return value
- daemon: Refactor signal pipe callback
- applet: application: Fix crash when processing deferred
- applet: application: Free libreport event configuration
- applet: application: Tweak style
- applet: application: Use GLib for logging
- dbus: Remove session objects when owner disconnects
- python-problem: Use org.freedesktop.Problems2
- Remove references to libreport Red Hat support integration
- gitignore: Add abrt_ccpp.catalog
- plugins: sosreport_event: Rename nfsserver plugin
- oops-utils: Better list iteration pattern
- oops-utils: Add an assertion
- a-dump-journal-core: Fix type mismatch and compariso
- rpm: Prevent null pointer dereference
- retrace-client: Make code style more consistent
- retrace-client: Don't check array for NULL
- dbus: Duplicate string passed to abrt_p2_object_new()
- github: check: Upload test log on failure
- github: check: Pass --load-average to make
- Add GitHub Actions workflow for building PRs
- applet: application: Fix style
- abrt-server: Fix double free and possible leaks
- Use g_autoptr where possible
- Remove GHashTable aliases
- Use g_autofree where possible
- Replace libreport_replace_map_string_item with g_hash_table_replace
- Replace libreport_next_map_string_iter with g_hash_table_iter_next
- Replace libreport_remove_map_string_item with g_hash_table_remove
- Replace libreport_get_map_string_item_or_NULL with g_hash_table_lookup
- Replace libreport_init_map_string_iter with g_hash_table_iter_init
- Replace libreport_get_map_string_item_or_empty with g_hash_table_lookup
- Replace libreport_free_map_string with g_hash_table_destroy
- Replace libreport_new_map_string with g_hash_table_new_full
- Replace libreport_xsetenv with g_setenv
- Replace libreport_xstat with g_stat
- Replace libreport_xpipe with g_unix_open_pipe
- Replace libreport_xchdir with g_chdir
- Replace libreport_concat_path_file with g_build_filename
- Replace strbuf with GString
- Replace libreport_prefixcmp with g_str_has_prefix
- Replace g_malloc with g_new for structs
- Replace libreport_xunlink with g_unlink
- Replace libreport_xopen with g_open
- Replace libreport_xzalloc with g_malloc0
- Replace libreport_xrealloc with g_realloc
- Replace libreport_xmalloc with g_malloc
- Replace libreport_xatou with g_ascii_strtoull
- Replace libreport_xatoi_positive with g_ascii_strtoull
- Replace libreport_xatoi with g_ascii_strtoll
- Replace libreport_xstrndup with g_strndup
- Replace libreport_xstrdup with g_strdup
- Replace libreport_xasprintf with g_strdup_printf
- init-scripts: Add directories to systemd-tmpfiles config
- abrt-console-notification: Work around noclobber
- abrt-console-notification: Swap redirects
- dbus: service: Fix Crash() emission
- runtests: mailx-reporting: Use a neutral time zone
- abrt-action-install-debuginfo: Fix variable reference
- runtests: bugzilla-comment-format: Quote path variables
- runtests: bugzilla-dupe-search: Quote path variables
- daemon: rpm: Use NEVRA instead of ENVRA
- runtests: symlinks: Quote variables
- doc: Clarify --insecure option a bit more
- doc: Remove or replace some fedorahosted links
- Drop creates-items tag from event definitions
- dbus: Update dd_get_owner
- abrtd: Drop unused variable
- applet: Chain up in dispose()
- applet: Pass instance pointer to signal handler
- sosreport_event: Update plugin name
- abrt-vmcore: Fix SyntaxWarning with python3.8
- abrtd: Don't delete new problem dirs
- abrt_get_backtrace: Move dir (un)locking to caller
- abrtd: Fix typo in function name
- make sure that former caches are group writable
- remove old transition postscriptlet
- setgid instead of setuid the abrt-action-install-debuginfo-to-abrt-cache [RHBZ 1796245]

* Mon May 11 2020 Michal Fabik <mfabik@redhat.com> 2.14.2-1
- auto-reporting: Fix missing libreport_ namespace
- Update translations
- github: coverity: Install dnf-plugins-core
- applet: application: Don’t truncate command lines with spaces
- applet: application: Enforce caller cleanup
- applet: application: Use org.freedesktop.Problems2 API
- applet: problem-info: Check component element
- applet: problem-info: Move D-Bus loading code
- applet: application: Don’t use g_application_get_default()
- applet: application: Use g_list_free_full()
- applet: problem-info: Destroy problem_data
- applet: application: Use GPtrArray
- applet: application: Properly split notification functions
- applet: application: Drop show_problem_list_notification()
- applet: application: Split off reporting from notifying
- Fix spelling in documentation
- github: coverity: Use Copr for libreport
- Bump libreport dependency

* Fri Apr 24 2020 Michal Fabik <mfabik@redhat.com> 2.14.1-1
- Allow editing CHANGELOG.md before release commit
- Remove libreport_ namespace from python code
- dbus: Drop bogus dependency
- dbus: Drop abrt_problems2
- plugins: journal: Fix ci_mapping being overwritten
- github: coverity: Upload build log
- Drop libcap dependency
- Update translations
- Add namespace to libreport function and global names
- github: coverity: Pass email address to web form
- github: coverity: Actually compress cov-int
- github: coverity: Install @rpm-development-tools
- github: coverity: Do add-path in a separate step
- github: coverity: Fix amending PATH
- Revert "github: coverity: Run find on /tmp"
- github: coverity: Run find on /tmp
- github: coverity: Fix PATH
- Revert "github: coverity: Specify path to cov-build"
- github: coverity: Fix secret for uploading
- github: coverity: Specify path to cov-build
- Revert "github: coverity: Run find after extraction"
- github: coverity: Run find after extraction
- github: coverity: Make wget less verbose
- github: coverity: Use secret properly
- github: coverity: Install wget
- github: coverity: Install intltool
- dont-blame-interpret: Rename and redo
- plugins: abrt-journal-core: Don’t assume anything about uid_t
- github: coverity: Don’t pipe yes to autogen.sh
- autogen.sh: Don’t prompt when installing dependencies
- github: coverity: Just use autogen.sh sysdeps
- libabrt: Rework function namespacing without #defines
- github: coverity: Don’t run configure too early
- autogen.sh: Handle NOCONFIGURE per the Build API
- Drop Travis config
- github: coverity: Install gettext-devel
- cli: Correct debug directories in config
- a-a-analyze-core: Remove useless comments, unused function
- lib,plugins: Accomodate for multiple debug directories
- github: Rename workflow
- Set up GitHub Actions
- .travis.yml: Update secret
- README.md: Add Coverity badge
- Add Travis CI config for Coverity builds
- plugins: xorg-utils: Loopify parsing
- plugins: xorg-utils: Shorten and explain condition
- plugins: xorg-utils: Drop outdated comment
- cli: Show defaults in help output
- cli: Fix verbosity option
- abrt-action-analyze-oops: Fix typo in translatable string
- tests: Add perl, php R and tcl to dont-blame-interpret
- a-a-save-package-data: Add R and tcl to interpreted langs
- a-a-save-package-data: Use regexps to match interpreters
- spec: Add %ghost for /var/run/abrt
- cli: Fix descriptions for --since and --until

* Fri Feb 07 2020 Ernestas Kulik <ekulik@redhat.com> 2.14.0-1
- Makefile.am: Don’t pull translations from Zanata
- README.md: Add Weblate badge
- plugins: abrt-action-analyze-oops: Refactor code
- lib: kernel: Simplify koops_extract_oopses() a bit
- tests: lib: Add a function that gets the first crash
- tests: Disable abrt-addon-upload-watch tests
- tests: Replace old service command with beakers commands
- dbus: Fix possibly uninitialized variable
- bodhi: Initialize karma values with default
- tests: Fix mailx-reporting tests
- tests: Do not stop abrt-journal-core service
- tests: Add short backtrace to CCpp crashes example
- plugins: Add short backtrace to ccpp catalog messages
- plugins: Bring back abrt_ccpp catalog to life
- dbus: Warn about large number of problems
- Fix build failure with gcc -fno-common
- Update translations
- Update abrt.pot
- koops: Avoid explicit dependency on hash size
- Update (uk) translation
- Update (de) translation
- Update (tr) translation
- Update (sk) translation
- Update (pt_BR) translation
- Update (hu) translation
- Update (fr) translation
- Update (cs) translation
- Update (bg) translation
- Translated using Weblate (Polish)
- Update translation files
- Stop using parse_list
- Move from nose to pytest
- Use GChecksum to compute checksums
- Drop scriptlets for abrt-ccpp.service
- Remove translation skeletons
- Add pot file
- tests: skip-sosreport-dir: Add missing prepare
- Update Makevars
- tests: runtests: run: Remove redundant shell command
- configure.ac: Fix indentation
- Move augeas lenses to new subdirectory
- Drop ChangeLog
- Remove development scripts
- events: Comment examples consistently
- cli: Disambiguate --authenticate help string
- cli: Print usage if option passed with no command
- pull-trans: Suggest zanata install
- shellcheck: Iterating over ls output is fragile. Use globs.
- shellcheck: Double quote to prevent globbing and word splitting
- zanata: Use python3 zanata client to pull translations
- Bump GLib dependency version to >=2.55.1
- python: Use correct paths for installed modules

* Fri Oct 11 2019 Matěj Grabovský <mgrabovs@redhat.com> 2.13.0-1
- cli: Use format argument in info command
- cli: Make pretty and format mutually exclusive
- cli: Set PYTHONPATH for tests
- cli: Rework commands
- Remove abrt-hook-ccpp
- spec: Use macros where appropriate
- tests: Use Augeas consistently
- spec: Move config files to corresponding subpackages
- build,spec: Empty config files in /etc/abrt/
- doc: Update man pages and comments in config files
- plugins: Update in-code defaults
- doc: Correct alignment of heading underlines
- a-dump-journal-core: Purge commented code
- dbus: Remove D-Bus configuration service
- Partly revert removal of default configs
- cli: Tweak problem matching
- plugins: Add satyr flags when building
- cli: Fix warning
- cli: Fix file name
- cli: Use decorator for MATCH positional argument
- cli: match: Iterate dict instead of calling keys()
- cli: Make pylint happier about imports
- cli: Drop unused variables
- cli: utils: Use consistent return
- cli: Improve i18n
- cli: Drop humanize import
- Drop uses of bind_textdomain_codeset()
- gitignore: Update path to generated file after cli-ng move
- tests: runner: Use systemctl instead of service
- autogen.sh: Use autoreconf

* Thu Aug 01 2019 Ernestas Kulik <ekulik@redhat.com> 2.12.2-1
- Remove old CLI and move cli-ng subpackage files into tui subpackage

* Wed Jul 03 2019 Martin Kutlak <mkutlak@redhat.com> 2.12.1-1
- Translation updates
- Rename all abrt-python to python3-abrt
- spec: Get rid of python provides
- hooks: abrt-hook-ccpp: Rename CreateCoreBacktrace setting
- Update icon
- non-fatal-mce: prepare oops1.test from template before using it
- meaningful-logs: check relative counts of lines instead of absolute
- oops-processing: fixed oops1.test handling. reworked so each oops has its own phase
- dumpoops: make sure hostname matches in oops_full_hostname.test
- Revert "applet: Add systemd service unit"
- a-a-analyze-c: Fix segfault when saving function name
- Fix grammar in implementation docs
- doc: Update man pages and mention locations of config files
- augeas,build,spec: Remove references to default configs
- daemon,lib: Update default config according to abrt.conf
- plugins: abrt-action-install-debuginfo: Replace uses of exit()
- plugins: abrt-action-install-debuginfo: Catch BrokenPipeError
- plugins: abrt-action-install-debuginfo: Remove unused imports
- daemon: Check return value after reading from channel
- spec: Require dbus-tools
- daemon: Don't process crashes from unpackaged apps by default
- plugins: abrt-action-install-debuginfo: Remove duplicate code
- plugins: abrt-action-install-debuginfo: Remove variable
- plugins: abrt-action-install-debuginfo: Remove comment
- plugins: abrt-action-install-debuginfo: Remove dead assignment
- doc: Fix spelling mistake in path to cache
- doc: Update man page for abrt-handle-upload
- doc: Update man page and help for a-a-install-debuginfo
- Nuke Python 2 support
- plugins: Catch unhandled exception in a-a-g-machine-id
- plugins: retrace-client: Bail out if we can’t get a config
- plugins: retrace-client: Default to HTTPS in RETRACE_SERVER_URI
- plugins: analyze_RetraceServer: Fix default RETRACE_SERVER_URI
- plugins: Fix name of env variable for retrace server
- configure.ac: Add dependency on libcap
- dbus: entry: Don’t claim truncation when there was none
- applet: Add systemd service unit
- spec: Remove obsolete scriptlets
- Makefile.am: Use correct locale when getting date

* Sat Feb 02 2019 Ernestas Kulik <ekulik@redhat.com> - 2.12.0-1
- Update to 2.12.0

* Tue Jan 08 2019 Matej Marusak <mmarusak@redhat.com> 2.11.1-1
- Translation updates
- Update CHANGELOG.md
- a-a-install-debuginfo: Clean cache if we need space
- shellcheck: Use command instead of type
- shellcheck: Check exit code directly with if mycmd
- shellcheck: Suppress shellcheck warning SC1090
- shellcheck: Use $(...) instead of legacy backticked
- cli: Add a shebang
- applet: Port to GApplication/GNotification
- spec: Add explicit package-version requirement of abrt-libs
- Introduce pylintrc
- augeas: Adjust testsuite to changes in libreport
- signal_is_fatal: generate problem reports for SIGSYS
- plugins: retrace-client: Port to libsoup
- plugins: retrace-client: Fix copy-paste error
- revert: spec: disable addon-vmcore on aarch64
- spec: turn on --enable-native-unwinder aarch64
- configure: Replace sphinx-build check with sphinx-build-3
- a-a-c-o-f-hw-error: Check systemd-journal for MCE logs
- koops: Filter kernel oopses based on logged hostname
- add hostname BR for tests
- add new prepare-data to dist files
- fix tests names for dist target after making templates from them
- fix for MCE events: Bug 1613182 - abrt-cli ignores rsyslog host info and incorrectly assumes that the receiving host had a problem
- dbus: Add configuration for Python3
- Add . to etc

* Mon Oct 08 2018 Martin Kutlak <mkutlak@redhat.com> 2.11.0-1
- Translation updates
- configure.ac: fix augparse typo
- plugins: Allow abrt-retrace-client to be optional at build time
- spec: Build python*-abrt-addon packages as noarch
- daemon: Fix double closed fd race condition
- gui: Replace deprecated g_type_class_add_private
- harvest_vmcore: Fix missing argument error during delete_and_close()
- sosreport: plugin "general" split into two new plugins
- plugins: Replace vfork with posix_spawn
- defattr is not needed since rpm 4.2
- python_sitearch -> python2_sitearch
- lib: Correct the syntax for gdb backtrace command
- gui-config: Remove deprecated GTK functions
- spec: remove duplicated python3-devel
- abrtd.service: force abrt-dbus to load changes from conf
- ccpp: add %h and %e parameter into abrt-hook-ccpp
- spec: set PYTHON variable because of ./py-compile

* Thu May 31 2018 Matej Marusak <mmarusak@redhat.com> 2.10.10-1
- Translation updates
- Changelog: Update changelog
- Remove dependency on deprecated nss-pem
- spec: abrt do not require python2
- spec: abrt-addon-ccpp do not require python2
- spec: drop python2-abrt-addon requires
- spec: fix bugs in python requires
- cores: comment an unclearing statement
- cores: print to stdout
- cores: read all journal files

* Fri Apr 27 2018 Matej Habrnal <mhabrnal@redhat.com> 2.10.9-1
- build: conditionalize the Python2 and Python3
- cli-ng,hooks,python-problem: Allow python to be optional at build time
- spec: fix ambiguous Python 2 dependency declarations
- plugins: a-a-g-machine-id use dmidecode command
- spec: use dmidecode instead of python3-dmidecode
- hooks: use container-exception-logger tool
- spec: container python hooks require cel
- hooks: do not write any additional logs
- a-a-s-package-data: add python3.7 to known Interpreters
- autogen: ignore abrt's python packages
- correctly parse buildrequires from spec file

* Wed Mar 21 2018 Matej Habrnal <mhabrnal@redhat.com> 2.10.8-1
- Translation updates
- spec: use Python3 as default in abrt-cli-ng
- cli-ng: use Python3 as default
- Add a new element 'interpreter' for python problems
- retrace-client: Require nss-pem

* Mon Feb 26 2018 Matej Habrnal <mhabrnal@redhat.com> 2.10.7-1
- Translation updates
- hooks: introduce docker hook for Python2
- hook: add type to Python3 container exception handler
- spec: introduce docker hook for Python2
- Add ABRT hexa stickers
- a-container-logger: workaround permission issue in minishift

* Mon Feb 19 2018 Matej Habrnal <mhabrnal@redhat.com> 2.10.6-1
- Translation updates
- hooks: introduce docker hook for Python3
- spec: introduce Python3 hook for container
- Remove deprecated is_error macro
- ldconfig is not needed in rawhide
- remove python_sitearch macro
- remove python_site macro
- move BuildRequires to top
- remove systemd-units and replace it with systemd macro
- remove init.d services
- a-h-event: Do not deduplicate different containers
- rpm: include epocho in package element if > 0

* Thu Nov 02 2017 Julius Milan <jmilan@redhat.com> 2.10.5-1
- Translation updates
- a-action-ureport: add option 'ProcessUnpackaged'
- spec: change dependency on python{2,3}-gobject
- applet: Additional changes to allow optional polkit
- doc: remove obsolete doxygen tags
- dbus: Additional changes to allow optional polkit
- cli-ng: Explicitly state python version in shebangs
- spec: rename python binary packages
- a-d-journal-core: Save mountinfo from journal
- a-d-journal-core: Save container cmdline
- logging: rename omitted log() to log_warning()

* Mon Aug 28 2017 Matej Habrnal <mhabrnal@redhat.com> 2.10.4-1
- Translation updates
- logging: rename log() to log_warning()
- Quick hack to fix build with rpm >= 4.14
- tests: Crash different binary in infinite event loop
- tests: Revert not sufficient fix
- tests: Reflect field changes in reporter-s-journal
- tests: Get docker-inspect while container is running
- cli,dbus: Allow polkit to be optional at build time
- spec: add dependency for python{3}-gobject
- a-d-journal-core: fix bad condition in creating reason msg
- a-d-journal-core: use pid of crashed process in dumpdir name
- changelog: update CHANGELOG.md

* Thu Jun 15 2017 Martin Kutlak <mkutlak@redhat.com> 2.10.3-1
- Translation updates
- applet: add a default action to a notification
- spec: require libreport-plugin-systemd-journal on Fedoras
- changing load location from bin to libexec
- changing location of abrt-action-save-container-data from bin to libexec
- koops: Improve not-reportable for oopses with taint flags
- This fixes #1173
- python: provide more information about exception
- abrt-journal: adapt to suspicious blacklist addition
- koops: add suspicious strings blacklist
- build: fix changelog adding in release target
- changelog: update CHANGELOG.md

* Tue Apr 25 2017 Matej Habrnal <mhabrnal@redhat.com> 2.10.2-1
- Translation updates
- spec: introduce migration to abrt-journal-core
- abrt_event: Save cpuinfo in problem directories
- koops: Improve fatal MCE check when dumping backtrace
- lib: typo in header
- Spelling fixes
- Python 3.6 invalid escape sequence deprecation fix
- koops_event: add check to restrict reporting of MCEs

* Thu Mar 16 2017 Matej Habrnal <mhabrnal@redhat.com> 2.10.1-1
- changelog: update CHANGELOG.md
- build: create tarball in release-* target
- spec: sosreport is not a package
- Fix Typo
- bodhi: Remove dependency on hawkey
- spec: Remove dependency on hawkey
- build: do not upload tarball to fedorahosted.org
- spec: do not use fedorahosted.org as source
- spec: install new plugins
- plugins: introduce Machine ID and SOS report
- Update CHANGELOG.md
- build: fix generating list of dependences in autogen.sh
- spec: start abrt-journal-core instead of abrt-ccpp
- build: fix scratch-build target
- a-a-ureport: fix calling of run_event_on_problem_dir
- spec: if using systemd, default to os-release ID for desktopvendor
- kernel: modify suspicious string "invalid opcode:"
- daemon: Allow rpm to be optional at build time
- spec: allow any compression of man pages
- spec: remove defattr
- spec: remove cleaning buildroot
- spec: use versioned provides
- spec: remove changelog entries older than 2 years
- remove Buildroot and Groups tags
- spec: recommend libreport-plugin-systemd-journal on Fedoras
- doc: document selinux change needed for automatic deletion of reports
- ccpp: tell gdb to analyze saved binary image

* Sat Dec 03 2016 Jakub Filak <jakub@thefilaks.net> 2.10.0-1
- Translation updates
- spec: bump required libreport and satyr versions
- build: make the release-* targets smarter
- Add CHANGELOG.md
- a-a-notify: set env var before run report_systemd-journal event
- use run_event_on_problem_dir() helper for running events
- notify: do not require package element
- spec: add catalog_journal_ccpp_format.conf file
- reporter-s-journal: add formatting file for abrt-journal-core analyser
- cli-ng: fix --fmt parameter
- python: create analyzer element in dumpdir
- abrt-action-list-dsos: fix typo in vendor variable name
- cli-ng: chown problem before reporting
- lib: stop printing out a debug message 'adding: '
- cli: print out the not-reportable reason
- cli: configure libreport to ignore not-reportable
- cli-ng: force reporting even if not-reportable
- cli-ng: introduce verbose argument
- Import GObject from gi.repository
- ccpp: configure package repositories for correct OS
- a-a-s-c-data: adapt to current docker
- daemon: don't drop problems from unknown containers
- a-a-s-c-data: correct detection of container type
- spec: install Bodhi event files
- bodhi: factor out Bodhi updates lookup into a solo event
- problems2: update the documentation
- a-a-analyze-python: create exception_type element
- a-a-analyze-xorg: create crash_function into dump dir
- koops: create crash_function element
- a-a-analyze-python: create crash_function element
- a-a-analyze-c: create crash_function element
- spec: add formatting files for reporter-systemd-journal
- reporter-systemd-journal: add formatting files
- vmcore: /var/tmp/abrt is no longer a dump location
- events: add event report_systemd-journal to all addons
- abrt-action-notify: notify to systemd journal
- spec: add abrt's catalog source files
- journal-catalog: add abrt's catalog source files
- ccpp: retain partial core_backtrace upon error
- ccpp: log waitpid errors
- ccpp: inform users about not supported unwinding
- ccpp: close stdin when we can let the process die
- daemon: properly shutdown socket connection
- daemon: close forgotten FD to /proc/[pid]
- ccpp: pass proc pid FD instead of pid to *_at fns
- ccpp+daemon: pass valid params to dd_open_item()
- python: remove unused functions from sysexcept hook
- build: add gettext-devel to sysdeps
- spec: add libcap-devel to BRs of addon-ccpp
- ccpp: avoid running elfutils under root
- Add abrt-action-analyze-vulnerability to .gitignore
- build: autoge.sh without args configures for debugging
- conf: increase MaxCrashReportsSize to 5GiB
- ccpp: fast dumping and abrt core limit
- CI: make debugging easier with more log messages
- doc: add a guide for ABRT hackers
- vmcore: fix an undefined variable on error path
- vmcore: read kdump.conf from an arbitrary location
- ccpp: use libreport 'at' functions
- ccpp: use abort() to exit in debug mode
- python2: stop generating dso_list in the process
- python: stop collecting ENVIRON in the process
- abrtd: details of processes from different PID NS
- abrtd: save interesting process details
- a-a-s-package-data: add python3.6 to known Interpreters
- spec: update gdb Requires
- tree-wide: make path to GDB configurable
- a-a-ureport: print out exit codes in verbose mode
- daemon: stop replacing analyzer with type

* Fri Sep 09 2016 Jakub Filak <jfilak@redhat.com> 2.9.0-1
- spec: install abrt_dbus_event.conf
- dbus: use Problems2 API in abrt-dbus
- dbus: Problems2 API implementation
- spec: install Problems2 interfaces
- dbus-doc: rewrite the XML to Problems2
- Fix memory leaks
- lib: introdcue a function checking post-create name
- abrtd: change HTTP response code for duplicate problems to 303
- autogen: fix typo in usage help string
- daemon: send base names from abrt-server to abrtd
- lib: normalize slashes of configured paths
- lib: make configuration paths alterable at runtime
- Add generated CCpp.conf to .gitignore
- abrt-bodhi: use CCpp PackageManager configuration directive from configure
- cli: introduce unsafe reporting for not-reporable problems
- handle-event: stop creating post-create lock
- daemon: trigger dump location cleanup after detection
- hook-ccpp: dump own core file in debug mode

* Mon Jul 18 2016 Matej Habrnal <mhabrnal@redhat.com> 2.8.2-1
- Translation updates
- abrt-hook-ccpp: Fix mismatching argument
- Allow selinux to be optional at build time
- vmcore: use findmnt to get mountpoint
- spec: add utils-linux to vmcore's Require
- vmcore: fix finding partitions by UUID and LABEL
- a-a-install-debuginfo: Exception may not have an argument errno
- koops: do not assume version has 3 levels
- Add ARM specific oops backtrace processing.
- examples: add oops-kernel-panic-hung-tasks-arm
- Add oops processing for kernel panics caused by hung tasks.
- abrt-hook-ccpp: save get_fsuid() return values in int variables

* Wed May 25 2016 Matej Habrnal <mhabrnal@redhat.com> 2.8.1-1
- a-dump-journal-xorg: allow *libexec/X* to be executable element
- a-dump-journal-xorg: add '_COMM=gnome-shell' to journal filter
- build: update pkg names for systemd
- a-d-journal-core: save core dump bytes from the journal field
- a-d-journal-core: support lz4 compressed core dump files
- a-a-install-debuginfo: do not try to split None
- doc: improve documentation of AllowedGroups, AllowedUsers and IgnoredPaths
- testcase: add serial field to uReport check
- a-a-install-debuginfo: correct handling of DebuginfoLocation
- a-a-s-container-data: update docker container ID parser
- abrt-hook-ccpp: drop saving of container env vars
- a-console-notification: do not leak variables
- a-retrace-client: format security
- daemon: avoid infinite crash loops
- spec: drop abrt-action-save-kernel-data bits
- spec: README -> README.md
- Add basic documentation
- a-a-install-debuginfo: fix BrokenPipe error
- a-a-install-debuginfo: make tmpdir variable global
- python3 addon: workaround a bug in traceback
- CCpp: turn off compat cores
- a-a-save-package-data: blacklist /usr/lib(64)/firefox/plugin-container
- Fix minor typo: possition -> position
- translations: add missing new line
- Translation updates
- translations: update zanata configuration
- ccpp: drop %e from the core_pattern
- Save Vendor and GPG Fingerprint

* Wed Feb 03 2016 Matej Habrnal <mhabrnal@redhat.com> 2.8.0-1
- a-a-save-package-data: do not blacklist firefox

* Tue Feb 02 2016 Matej Habrnal <mhabrnal@redhat.com> 2.7.2-1
- ccpp: bug fix - undefined variables
- a-a-c-o-f-hw-error: fix unicode error
- ccpp: use error_msg_ignore_crash() instead of error_msg()
- ccpp: add AllowedUsers and AllowedGroups feature
- doc: fix formatting in abrt.conf man page
- ccpp: use executable name from pid
- a-a-c-o-f-hw-error: do not crash on invalid unicode
- Use %s instead of %d.
- configui: link GUI library with libabrt.so
- Do not include system libabrt.h
- ccpp: unify log message of ignored crashes
- ccpp: add IgnoredPath option
- lib: check_recent_crash_file do not produce error_msg

* Mon Nov 23 2015 Jakub Filak <jfilak@redhat.com> 2.7.1-1
- spec: switch owner of the dump location to 'root'
- abrtd: switch owner of the dump location to 'root'
- lib: add convenient wrappers for ensuring writable dir
- ccpp: save abrt core files only to new files
- ccpp: ignore crashes of ABRT binaries if DebugLevel == 0
- conf: introduce DebugLevel
- a-a-i-d-to-abrt-cache: make own random temporary directory
- update .gitignore
- ccpp: make crashes of processes with locked memory not-reportable
- a-a-s-p-data: fix segfault if GPGKeysDir isn't configured
- a-dump-journal-xorg: make journal filter configurable
- doc: a-a-analyze-xorg fix path to conf file
- abrt-journal: use GList instead of char** in abrt_journal_set_journal_filter()
- xorg: introduce tool abrt-dump-journal-xorg
- abrt-xorg.service: change due to abrt-dump-journal-xorg
- journal: add function abrt_journal_get_next_log_line
- spec: add abrt-dump-journal-xorg to spec file
- xorg: rewrite skip_pfx() function to work with journal msgs
- xorg: introduce library xorg-utils
- dbus: ensure expected bytes width of DBus numbers
- a-d-journal-core: set root owner for created dump directory
- doc: add missing man page for abrt-dump-journal-core
- spec: add missing man page for abrt-dump-journal-core

* Thu Oct 15 2015 Matej Habrnal <mhabrnal@redhat.com> 2.7.0-1
- abrt-python: add problem.chown
- a-a-a-ccpp-local don't delete build_ids
- update .gitignore
- spec: add cli-ng
- cli-ng: initial

* Thu Oct 15 2015 Matej Habrnal <mhabrnal@redhat.com> 2.6.3-1
- bodhi: introduce wrapper for 'reporter-bugzilla -h' and 'abrt-bodhi'
- remove random code example from abrt-server
- spec: introduce abrt-action-find-bodhi-update
- api: fix pths -> paths rename
- handle-event: remove obsolete workaround
- remove 'not needed' code
- events: fix example wording
- doc: change /var/tmp/abrt to /var/spool/abrt
- doc: actualize core_pattern content in documentation
- doc: fix default DumpLocation in abrt.conf man page
- events: improve example
- events: comments not needed anymore
- abrt-retrace-client: use atoll for _size conversion
- abrt-dump-xorg: support Xorg log backtraces prefixed by (EE)
- runtests: more verbose fail in get_crash_path
- ureport-auth: force cp/mv when restoring configuration
- runtests: stick to new BZ password rules
- bodhi: fix typo in error messages
- bodhi: fix a segfault when testing an os-release opt for 'rawhide'
- doc: actualize the abrt-bodhi man page
- autogen: use dnf instead of yum to install dependencies
- bodhi: add parsing of error responses
- bodhi: add ignoring of Rawhide
- ccpp: do not break the reporting if a-bodhi fails
- spec: add hawkey to BRs of abrt-bodhi
- introduce bodhi2 to abrt-bodhi
- a-handle-upload: pass bytes to file.write()
- upload a problem data in the EVENT 'notify'
- turn off several post-create scripts for remote problems
- convert all 'ex.message' stmts to 'str(ex)'
- cli: don't start reporting of not-reportable problems
- a-a-s-p-d: add bash on the package blacklist
- correct usage of abrt-gdb-exploitable
- testsutie: first wait_for_hooks, then get_crash_path
- ccpp: use global TID
- ccpp: fix comment related to 'MakeCompatCore' option in CCpp.conf
- cli: fix testing of DBus API return codes
- dbus-api: unify reporting of errors
- doc: fix related to conditional compilation of man page
- abrt-auto-reporting: fix related to conditional compilation
- vmcore: read vmcore by chunks
- pass encoded Unicode to hashlib.sha1.update()
- abrt-merge-pstoreoops: merge files in descending order
- use gettext instead of lgettext in all python scripts
- gitignore: add a generated man page source file

* Fri Jul 17 2015 Jakub Filak <jfilak@redhat.com> 2.6.2-1
- applet: do not crash if the new problem has no command_line
- ccpp: do not crash if generate_core_backtrace fails
- abrt: Fixup component of select kernel backtraces
- abrtd: de-prioritize post-create event scripts
- spec: switch python Requires to python3
- switch all python scripts to python3
- spec: drop abrt-addon-python requires
- a-dump-oops: allow update the problem, if more then one oops found
- cli: use internal command impl in the command process
- cli: remove useless code from print_crash()
- cli: enable authetication for all commands

* Thu Jul 02 2015 Matej Habrnal <mhabrnal@redhat.com> 2.6.1-1
- dbus: keep the polkit authorization for all clients
- cli: enable polkit authentication on command line
- spec: --enable-dump-time-unwind by default
- ccpp: use TID to find crash thread
- spec: remove PyGObject from all Requires
- spec: update version of gdb because of -ascending
- lib: make it easier to find the backtrace of th crash thread
- ccpp: save TID in the file 'tid'
- ccpp: get TID from correct cmd line argument
- configui: add option always generate backtrace locally
- a-a-p-ccpp-analysis: use ask_yes_no_save_result instead of ask_yes_no_yesforever
- spec: use more appropriate url
- spec: abrt requires libreport-plugin-rhtsupport on rhel
- sosreport: add processor information to sosreport
- doc: update abrt-cli man page

* Tue Jun 09 2015 Jakub Filak <jfilak@redhat.com> 2.6.0-1
- spec: add abrt-dbus to Rs of abrt-python
- vmcore: use libreport dd API in the harvestor
- ccpp: don't save the system logs by default
- cli: exit with the number of unreported problems
- spec: restart abrt-dbus in posttrans
- cli: chown before reporting
- hooks: use root for owner of all dump directories
- ccpp: do not unlink failed and big user cores
- ccpp: include the system logs only with root's coredumps
- koops: don't save dmesg if kernel.dmesg_restrict=1
- daemon, dbus: allow only root to create CCpp, Koops, vmcore and xorg
- daemon: allow only root user to trigger the post-create
- daemon: harden against race conditions in DELETE
- ccpp: revert the UID/GID changes if user core fails
- a-a-i-d-t-a-cache: sanitize umask
- a-a-i-d-t-a-cache: sanitize arguments
- dbus: report invalid element names
- dbus: avoid race-conditions in tests for dum dir availability
- dbus: process only valid sub-directories of the dump location
- lib: add functions validating dump dir
- daemon: use libreport's function checking file name
- configure: move the default dump location to /var/spool
- ccpp: avoid overriding system files by coredump
- spec: add libselinux-devel to BRs
- ccpp: emulate selinux for creation of compat cores
- ccpp: harden dealing with UID/GID
- ccpp: do not use value of /proc/PID/cwd for chdir
- ccpp: do not override existing files by compat cores
- ccpp: stop reading hs_error.log from /tmp
- ccpp: fix symlink race conditions
- turn off exploring crashed process's root directories
- abrt-python: add proper PYTHONPATH to test shellscripts
- abrt-python: unify unknown problem type handling
- abrt-python: add not_reportable properties
- spec: remove analyzer to type conversion
- abrt-python: add Python3 problem type
- abrt-python: add id, short_id and path to problem
- abrt-python: add Problem.prefetch_data function
- abrt-python: handle reconnection gracefully
- config UI: Automatic reporting from GSettings
- doc, polkit: Spelling/grammar fixes
- applet: fix problem info double free
- a-a-s-p-d: add new known interpreter to conf file
- config UI: enable options without config files
- config UI: read glade from a local file first
- applet: migrate Autoreporting options to GSettings
- abrt-action-list-dsos: do not decode not existing object
- spec: add AUTHENTICATED_AUTOREPORTING conditional
- abrt-auto-reporting: require rhtsupport.conf file only on RHEL
- lib: add new kernel taint flags
- spec: add a dependency on abrt-dbus to abrt-cli
- cli: do not exit with segfault if dbus fails
- applet: switch to D-Bus methods
- upload: validate and sanitize uploaded dump directories

* Thu Apr 09 2015 Jakub Filak <jfilak@redhat.com> 2.5.1-1
- Translation updates
- problem: use 'type' element instead of 'analyzer'
- cli-status: don't return 0 if there is a problem older than limit
- journal-oops: add an argument accepting journal directory
- journal: open journal files from directory
- lib: don't expect kernel's version '2.6.*' or '3.*.*'
- cli: use the DBus methods for getting problem information
- libabrt: add wrappers TestElemeExists and GetInfo for one element
- dbus: add new method to test existence of an element
- libabrt: add new function fetching full problem data over DBus
- applet: use a shared function for getting problems over DBus
- vmcore: generate 'reason' file in all cases
- applet: Fix trivial indentation bug
- applet: Don't show report button for unpackaged programs
- applet: fix freeing of the notify problem list
- applet: get the list of problems through D-Bus service
- doc: D-Bus api: make desc of DeleteProblem clearer

* Wed Mar 18 2015 Jakub Filak <jfilak@redhat.com> 2.5.0-1
- applet: cast to correct type to fix a warrning
- applet: Use new problem_create_app_from_env() helper
- doc: add documentation for GetProblemData
- dbus: add a new method GetProblemData
- abrt_event: run save package data event even if component exists
- a-a-s-container-data: add a new argument --root
- spec: add a-a-s-package-data to abrt-atomic
- a-a-s-kernel-data: add --root argument
- journal-oops: add an argument similar to '--merge'
- spec: let configure generate the spec file
- ccpp: create the dump location from standalone hook
- retrace-client: stop failing on SSL2
- spec: changes for Atomic hosts
- add stuff necessary for Project Atomic
- Python 3 fixes
- ccpp: add support for multiple pkg mngrs
- Python 3 compatibility
- Revert "dbus: Allow admins to load problems without a password"
- dbus: Allow admins to load problems without a password
- abrtd: Don't allow users to list problems "by hand"
- spec: Don't allow users to list problems "by hand"
- spec: abrt-python requires libreport-python to build

* Fri Feb 20 2015 Jakub Filak <jfilak@redhat.com> 2.4.0-1
- spec: factor out core_pattern helper from addon-ccpp
- ccpp: standalone hook
- ccpp: save package data from hook in case of crash in container
- a-a-s-package-data: save data from artifical chroots
- spec: install containers tools
- containers: add utility collecting containers info
- ccpp: add support for containers
- spec: install the daemon's D-Bus configuration file
- daemon: add configuration enabling our name on the System bus
- daemon: get rid of own main loop
- init: set Type of abrtd.service to dbus
- applet: Use libreport's helper to find applications
- applet: Remove unused build information
- build: Fix pkg-config warning related to abrt.pc
- applet: Fix a massive leak in the app detection code
- applet: Remove left-over code from the systray icon
- applet: Use the easy way to detect empty lists
- applet: Fix a number of "problems" memory leaks
- applet: Make problem_info_t refcounted
- applet: If gnome-abrt isn't there, don't offer to report
- applet: Fix multiple notifications for the same problem
- applet: Always defer auto-reporting without a network
- applet: Don't ignore foreign problems if an admin
- applet: Rename problem variable to "pi"
- applet: Remove unused "flags" parameters
- applet: Completely ignore incomplete problems
- applet: Don't ignore repeat problems in the same app
- applet: Fix warning when crash doesn't happen in app
- applet: Remove unused functions
- applet: Remove unused flags
- applet: Rewrite notifications
- applet: Don't run full reports from the applet
- applet: Simplify "report" action
- applet: Add helper to guess .desktop for a cmdline
- applet: Get more details from the crash report
- applet: Ignore other people's problems for non-admins
- applet: Remove handling of "ignored" crashes
- applet: Remove specific persistent notifications handling
- applet: Rename applet to match gnome-abrt
- applet: Initialise libnotify on startup
- applet: Use g_new0() instead of xzalloc()
- applet: Use g_strdup_printf()/g_strdup()
- applet: Move variable inside block where it's used
- daemon: process unpackaged by default
- spec: fix abrt-applet requires
- applet: Fix memory leak in fork_exec_gui()
- applet: Detect whether gnome-abrt is available
- applet: Use GUI_EXECUTABLE macro
- autogen: move configure to the default case
- applet: Use GIO to launch gnome-abrt
- applet: Fix typo in "Oterwise"
- applet: Use symbolic icon instead of abrt's in notifications
- applet: Add some debug to new_dir_exists()
- applet: Require at least libnotify 0.7
- applet: Fix typo in "cuurent"
- applet: Don't defer sending out uReports
- applet: Use G_SOURCE_REMOVE in timeout callback
- spec: Bump required glib2 version
- applet: Use g_bus_own_name() for single-instance
- applet: Remove status icon
- applet: Use GDBus to filter crash signals
- applet: Remove XSMP support
- build: Launch configure after autogen.sh
- make: make some python depencies optional
- configure: fix typos
- configure: check for python-sphinx and nose
- spec: add gsettings-desktop-schemas to the build requires
- core: use updated dump_fd_info()
- switch from 'analyzer' to 'type'
- spec: install abrt-dump-journal-core stuff
- init: add abrt-journal-core service
- introduce abrt-dump-journal-core
- applet: Remove the automatic crash reporting message dialog
- applet: Remove pre-glib 2.32 code
- applet: Remove pointless custom signal handling
- applet: Use GNetworkMonitor instead of NM directly
- applet: Use GSettings to check whether to send uReports
- Rewrite journalctl invocations: replace grep/tail pipeline with journalctl builtins.
- Don't slurp unbounded amounts of data when invoking journalctl. Fixes #887.
- console-notifications: add timeout
- cli-status: use will_python_exception
- ccpp-hook: move utility functions to hooklib
- ccpp-hook: move /proc/[pid]/ utils to libreport
- abrt-journal: add functions for reading/saving journald state
- Do not use 'bool' in OPT_BOOL() macro : it expects 'int'
- daemon: Own a D-Bus name
- zanata: add gettext mappings
- auto-reporting: add options to specify auth type
- translations: move from transifex to zanata
- spec: add missing augeas dependency
- Only analyze vulnerabilities when coredump present
- abrt-install-ccpp-hook check configuration
- UUID from core backtrace if coredump is missing
- Create core backtrace in unwind hook
- abrt-hook-ccpp: minor refactoring
- vmcore: remove original vmcore file in the last step
- vmcore: catch IOErrors and OSErrors
- python: load the configuration from correct file
- Remove garbage from ccpp_event.conf
- spec: update the required gdb version
- gdb: make gdb aware of the abrt's debuginfo dir
- Revert "gdb: disable loading of auto-loaded files"
- spec: update the URL
- koops: improve 'reason' text for page faults
- sos: use all valuable plugins
- a-a-g-machine-id: do not print any error from the event handler
- a-a-g-machine-id: omit trailing new-line for one-liners only
- a-a-g-machine-id: suppress its failures in abrt_event.conf
- a-a-g-machine-id: add systemd's machine id
- applet: ensure writable dump directory before reporting
- make ABRT quieter
- journal-oops: use the length result of sd_journal_get_data()
- console-notifications: skip non-interactive shells
- applet: don't show duphash instead of component
- ureport: attach contact email if configured
- console-notifications: use return instead of exit
- Translation updates
- a-a-s-p-d: add firefox on the package blacklist
