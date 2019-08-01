# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

## [2.12.2]
### Changed
- abrt-cli was removed in favor of abrt-cli-ng

### Fixed
- Made the Python API register a text authentication agent for Polkit for privileged D-Bus calls
- Fixed abrt-cli-ng commands and improved their compatibility with abrt-cli
- Improved compatibility with RPM 4.14

## [2.12.1]
### Added
- Require dbus-tools for dbus-send command.

### Changed
- ABRT won't process crashes coming from unpackaged applications by default.
- Completely removed support for Python2.
- Update new ABRT icon.
- retrace-client defaults to https.
- Updated man pages.
- Removed configurations from /usr/share/abrt.
- Removed provides for abrt-python packages.

### Fixed
- Gracefully handle exceptions comming from abrt-action-generate-machine-id.
- Gracefully handle exceptions comming from abrt-action-install-debuginfo.
- Use correct name of environment variable for retrace-server.


## [2.12.0]
### Added
- Print message when debug info cache is being cleaned
- Add support for exporting workflows used in reporting over D-Bus

### Changed
- Print error message instead of triggering an exception in abrt-action-find-bodhi-update
- Adapt to changes in libreport API

### Fixed
- Fix user after free when creating problem directory structure
- Fix buffer overrun when printing help output in abrt-dump-journal-xorg

## [2.11.1]
### Added
- Dbus configuration for Python3
- abrt-cli does not ignore rsyslog host info anymore and incorrectly assumes
that the receiving host had a problem
- Filter kernel oopses based on logged hostname as logs can be collected from
multiple machines
- Add systemd-journal to places to check for MCE logs
- Turn on native-unwinder for aarch64
- Generate problem reports for SIGSYS
- Clean cache if more space is needed

### Changed
- Port retrace-client to libsoup
- Port applet to GApplication/GNotification
- ShellCheck fixes

## [2.11.0]
### Changed
- Architecture of python[2,3]-abrt-addon packages was changed to noarch.
- Restarting abrtd daemon forces abrt-dbus to reload configuration files.
### Fixed
- Fixed issue with local backtrace generation.
- Fixed bug that caused daemon to consume 100% of CPU core.
- Fixed issue with abrt not resolving a hostname in coredump filenames.

## [2.10.10]
### Removed
- Requirements on Python 2
- Dependency on nss-pem

## [2.10.9]

## [2.10.8]

## [2.10.7]

## [2.10.6]

## [2.10.5]
### Added
- Added saving of container cmdline.
- Added saving mountinfo from journal.
- Added processing of unpackaged executables.

## [2.10.4]

## [2.10.3]
### Added
- Added blacklist for suspicious kernel oops messages to filter out false
positives.
- Added descriptions for taint flags of kernel oopses.
- Added default action for notifications. Clicking on a notification now
opens 'gnome-abrt' with the detected problem.

### Changed
- Translation updates.
- 'abrt-addon-python' and 'abrt-addon-python3' now provide more information
about where the exception happened.

### Fixed

## [2.10.2]
### Added
- Added creation of new element 'mce' for oopses, where it's possible values
are 'fatal' or 'non-fatal' and check to restrict reporting of MCEs.
- Start saving cpuinfo in problem directories.
- Introduce migration to abrt-journal-core.

### Changed
- Improve fatal MCE check when dumping backtrace. Problem was, that regular
kernel panic message was treated as fatal MCE, for which we don't want to have
backtrace.
- Translation updates.

### Fixed
- Python 3.6 invalid escape sequence deprecation fix and some spelling fixes.

## [2.10.1]
### Added

### Changed
- 'abrt-ccpp.service' is not enabled by default.
'abrt-abrt-journal-core.service' is enablen instead. ABRT gets coredumps from
systemd journal.
- Modify suspicious kernel string "invalid opcode:" because "invalid opcode:"
can also be without colon.
- The EVENTs generating machine-id and executing sosreport are now delivered as
standalone plugins.
- Remove dependency on hawkey. Function hy_split_nevra of obsolete hawkey
library replaced by own implementation, as hawkey will be removed soon.

### Fixed
- Fix calling of 'run_event_on_problem_dir'. The function is imported as a solo
identifier from the report module.
- Fix scratch-build targe. Prefix "dist-" in no longer used in koji build target.


## [2.10.0]
### Added
- Start reporting the detected problems to systemd-journal in the form of
catalogue messages that contains essential problem details. The messages are
reported with SYSLOG_IDENTIFIER=abrt-notification and are mainly designated for
developers and administrator.
- Start capturing /proc/[pid]/ns details and some other interesting process
details for uncaught Python, Ruby and Java exceptions in the problem data.
- Run the core dump time backtrace generator under the user of the crashed
process and not under root. This change makes ABRT more secure to deploy as
ABRT no longer runs elfutils functions in superuser context.
- Set up the yum/dnf debuginfo repositories according to /etc/os-release
captured in the problem details, so it is possible to analyze core dump files
of processes running in a container or a changed root environment.
- Save core dump files to disk using low level kernel functions to make the
dumping a little bit faster.
- Start limiting the dumped core file size and set the default limit to 5GiB.
The limit can be changed through the MaxCoreFileSize configuration option in
the /etc/abrt/plugins/CCpp.conf file.
- Give the Kernel vmcore plugin the ability to parse the kdump.conf file at any
location, so the plugin can be used from a container.

### Changed
- Move the look for Bodhi updates including know Bugzilla bugs to a solo event
to allow users of non-Fedora distributions to run the core dump analysis tools
without error messages about unavailability of Bodhi.
- Start notifying all detected problems and not only those that are related to
a package.
- Update the list of known interpreters with python3.6.

### Fixed
- Ensure that the reporting will not be terminated with the 'the problem cannot
be reported' error message when a user passes the '--unsafe' argument on
command line.
- Start considering child processes of the processes that run binaries with the
path prefix '/usr/libexec/docker' containerized by Docker.
- Correct a typo in a name of a variable causing the absence of DSO list
(Dynamic Shared Objec list) in the captured problem details.
- Enable usage of the problem Python API in GObject projects by proper includes
from gi.repository.
- Make sure all users can run `abrt report` on their problems and the process
does not exit with file access permission error.
- Fix the bugs preventing users from passing their preferred format of problem
data to `abrt list --fmt` and `abrt info --fmt`.
- Fix several file descriptor leaks in abrtd.


[Unreleased]: https://github.com/abrt/abrt/compare/2.12.2...HEAD
[2.12.2]: https://github.com/abrt/abrt/compare/2.12.1...2.12.2
[2.12.1]: https://github.com/abrt/abrt/compare/2.12.0...2.12.1
[2.12.0]: https://github.com/abrt/abrt/compare/2.11.1...2.12.0
[2.11.1]: https://github.com/abrt/abrt/compare/2.11.0...2.11.1
[2.11.0]: https://github.com/abrt/abrt/compare/2.10.10...2.11.0
[2.10.10]: https://github.com/abrt/abrt/compare/2.10.9...2.10.10
[2.10.9]: https://github.com/abrt/abrt/compare/2.10.8...2.10.9
[2.10.8]: https://github.com/abrt/abrt/compare/2.10.7...2.10.8
[2.10.7]: https://github.com/abrt/abrt/compare/2.10.6...2.10.7
[2.10.6]: https://github.com/abrt/abrt/compare/2.10.5...2.10.6
[2.10.5]: https://github.com/abrt/abrt/compare/2.10.4...2.10.5
[2.10.4]: https://github.com/abrt/abrt/compare/2.10.3...2.10.4
[2.10.3]: https://github.com/abrt/abrt/compare/2.10.2...2.10.3
[2.10.2]: https://github.com/abrt/abrt/compare/2.10.1...2.10.2
[2.10.1]: https://github.com/abrt/abrt/compare/2.10.0...2.10.1
[2.10.0]: https://github.com/abrt/abrt/compare/2.9.0...2.10.0
