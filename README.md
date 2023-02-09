[![Translation status](https://translate.fedoraproject.org/widgets/abrt/-/abrt/svg-badge.svg)](https://translate.fedoraproject.org/engage/abrt/)
[![Build status](https://github.com/abrt/abrt/actions/workflows/check.yml/badge.svg)](https://github.com/abrt/abrt/actions/workflows/check.yml)
[![Coverity Scan build status](https://scan.coverity.com/projects/17423/badge.svg)](https://scan.coverity.com/projects/abrt-abrt)

# ABRT

**A set of tools to help users detect and report application crashes.**

### About

Its main purpose is to ease the process of reporting an issue and finding a
solution.

The solution in this context might be a bugzilla ticket, knowledge base article
or a suggestion to update a package to a version containing a fix.

This repository is one among a suite of related projects. The following diagram
summarizes the dependencies between the individual packages comprising the ABRT
suite.

```mermaid
flowchart BT
    abrt-java-connector --> abrt
    abrt-java-connector -. build .-> satyr
    abrt:::focus --> libreport & satyr
    abrt-java-connector --> libreport
    gnome-abrt --> abrt & libreport
    reportd --> libreport
    libreport --> satyr
    retrace-server[Retrace Server] -. "optional, for<br>packages only" .-> faf
    faf["ABRT Analytics (FAF)"] --> satyr

click abrt "https://github.com/abrt/abrt" "abrt GitHub repository" _blank
click abrt-java-connector "https://github.com/abrt/abrt-java-connector" "abrt-java-connector GitHub repository" _blank
click faf "https://github.com/abrt/faf" "ABRT Analytics GitHub repository" _blank
click gnome-abrt "https://github.com/abrt/gnome-abrt" "gnome-abrt GitHub repository" _blank
click libreport "https://github.com/abrt/libreport" "libreport GitHub repository" _blank
click reportd "https://github.com/abrt/reportd" "reportd GitHub repository" _blank
click satyr "https://github.com/abrt/satyr" "satyr GitHub repository" _blank
click retrace-server "https://github.com/abrt/retrace-server" "Retrace Server GitHub repository" _blank

classDef focus stroke-width: 4
```

### Documentation

Every ABRT program and configuration file has a man page describing it. It is
also possible to [read the ABRT documentation](http://abrt.readthedocs.org/)
online. For contributors and developers, there are also [wiki
pages](https://github.com/abrt/abrt/wiki) describing some topics to deeper
technical details.

### Development

 * IRC Channel: #abrt on [irc.libera.chat](https://libera.chat/)
 * [Mailing List](https://lists.fedorahosted.org/admin/lists/crash-catcher.lists.fedorahosted.org/)
 * [Bug Reports and RFEs](https://github.com/abrt/abrt/issues)
 * [Contributing to ABRT](CONTRIBUTING.md)
 * [Install and run ABRT](INSTALL.md)


### Running

ABRT consist of several services and many small utilities. While The utilities
can be successfully run from the source directories after build, the services
often uses the utilities to do actions and expect the utilities installed in
the system directories. Hence to run the services, it is recommended to install
ABRT first and run them as system services. The instructions how to build
and install ABRT can be found in [INSTALL.md](INSTALL.md)

### Technologies

* [libreport](https://github.com/abrt/libreport) - problem data format, reporting
* [satyr](https://github.com/abrt/satyr) - backtrace processing, micro-reports
* [Python3](https://www.python.org/)
* [GLib2](https://developer.gnome.org/glib/)
* [Gtk3](https://developer.gnome.org/gtk3)
* [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/)
* [SELinux](https://github.com/SELinuxProject/selinux/wiki)
* [systemd](https://www.freedesktop.org/wiki/Software/systemd/)
