Name:		my_crash
Version:	0.0
Release:	1%{?dist}
Summary:	I can crash

Group:		Development/Tools
License:	GPLv3
URL:		http://my_crash-rpm.cz
Source0:	%{name}/%{name}-%{version}.tar.gz
Packager:   Matej Habrnal <mhabrnal@redhat.com>
Vendor:     abrt

%description
All binary files in this package crash.

%global debug_package %{nil}

%prep
%setup -q -c

%build
echo build
gcc usr/sbin/ccpp_crash.c -o usr/sbin/ccpp_crash

%install
echo "install"
rm -rf $RPM_BUILD_ROOT
mkdir -p  $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}/usr/sbin
cp usr/sbin/ccpp_crash  ${RPM_BUILD_ROOT}/usr/sbin/ccpp_crash
cp usr/sbin/python_crash  ${RPM_BUILD_ROOT}/usr/sbin/python_crash

%files
%defattr(-,root,root,-)
%{_sbindir}/ccpp_crash
%{_sbindir}/python_crash

%changelog
