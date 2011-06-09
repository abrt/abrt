%{!?python_site: %define python_site %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(0)")}
# platform-dependent
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

Summary: Generic library for reporting various problems
Name: libreport
Version: 2.0.2
Release: 1%{?dist}
License: GPLv2+
Group: System Environment/Libraries
URL: https://fedorahosted.org/abrt/
Source: https://fedorahosted.org/released/abrt/%{name}-%{version}.tar.gz
BuildRequires: dbus-devel
BuildRequires: gtk2-devel
BuildRequires: curl-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: desktop-file-utils
BuildRequires: libnotify-devel
BuildRequires: xmlrpc-c-devel
#why? BuildRequires: file-devel
BuildRequires: python-devel
BuildRequires: gettext
BuildRequires: libxml2-devel
BuildRequires: libtar-devel
BuildRequires: intltool
BuildRequires: libtool
BuildRequires: nss-devel
BuildRequires: texinfo
BuildRequires: asciidoc
BuildRequires: xmlto

# for rhel6
%if 0%{?rhel} >= 6
BuildRequires: gnome-keyring-devel
%else
BuildRequires: libgnome-keyring-devel
%endif

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Libraries providing API for reporting different problems in applications
to different bug targets like Bugzilla, ftp, trac, etc...

%package devel
Summary: Development libraries and headers for libreport
Group: Development/Libraries
Requires: libreport = %{version}-%{release}

%description devel
Development libraries and headers for libreport

%package python
Summary: Python bindings for report-libs
# Is group correct here? -
Group: System Environment/Libraries
Requires: libreport = %{version}-%{release}
Provides: report > 0.20
# FIXME: just a workaround to make it work with python-meh, but we should probably provide -newt UI asap
Provides: report-newt > 0.20
Obsoletes: report <= 0.20

%description python
Python bindings for report-libs.

%package gtk
Summary: GTK front-end for libreport
Group: User Interface/Desktops
Requires: libreport = %{version}-%{release}
Provides: report-gtk > 0.20
Obsoletes: report-gtk <= 0.20

%description gtk
Applications for reporting bugs using libreport backend

%package gtk-devel
Summary: Development libraries and headers for libreport
Group: Development/Libraries
Requires: libreport-gtk = %{version}-%{release}
Provides: report-gtk > 0.20
Obsoletes: report-gtk <= 0.20

%description gtk-devel
Development libraries and headers for libreport-gtk

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
mkdir -p $RPM_BUILD_ROOT/var/spool/abrt

# After everything is installed, remove info dir
rm -f %{buildroot}%{_infodir}/dir

%clean
rm -rf $RPM_BUILD_ROOT

%post gtk
/sbin/ldconfig
# update icon cache
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%postun gtk
/sbin/ldconfig
if [ $1 -eq 0 ] ; then
    touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans gtk
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc README COPYING
%dir %{_sysconfdir}/abrt/
%config(noreplace) %{_sysconfdir}/abrt/report_event.conf
%{_libdir}/libreport.so.*
%{_libdir}/libabrt_dbus.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/libreport/abrt_dbus.h
%{_includedir}/libreport/dump_dir.h
%{_includedir}/libreport/event_config.h
%{_includedir}/libreport/hash_sha1.h
%{_includedir}/libreport/libreport.h
%{_includedir}/libreport/libreport_problem_data.h
%{_includedir}/libreport/libreport_types.h
%{_includedir}/libreport/logging.h
%{_includedir}/libreport/parse_options.h
%{_includedir}/libreport/problem_data.h
%{_includedir}/libreport/read_write.h
%{_includedir}/libreport/report.h
%{_includedir}/libreport/run_event.h
%{_includedir}/libreport/strbuf.h
%{_includedir}/libreport/xfuncs.h
%{_libdir}/libreport.so
%{_libdir}/libabrt_dbus.so
%{_libdir}/pkgconfig/libreport.pc

%files python
%defattr(-,root,root,-)
%{python_sitearch}/report/*

%files gtk
%defattr(-,root,root,-)
%{_bindir}/bug-reporting-wizard
%{_libdir}/libreport-gtk.so.*

%files gtk-devel
%defattr(-,root,root,-)
%{_libdir}/libreport-gtk.so
%{_includedir}/libreport/libreport-gtk.h
%{_libdir}/pkgconfig/libreport-gtk.pc

%changelog
* Wed Jun 01 2011 Jiri Moskovcak <jmoskovc@redhat.com> 2.0.2-1
- initial packaging
