.. _properties:

Problem object properties
=========================

Currently, there is no strict specification of problem properties
and you are free to add your own data as you see fit
(log files, process data) provided you are planning to use
them for reporting.

Mandatory properties required prior saving:

===================== ======================================================== ====================
Property              Meaning                                                  Example
===================== ======================================================== ====================
``executable``        Executable path of the component which caused the        ``'/usr/bin/time'``
                      problem.  Used by the server to determine
                      ``component`` and ``package`` data.
===================== ======================================================== ====================

Following properties are added by the server when new problem is
created:

===================== ======================================================== ====================
Property              Meaning                                                  Example
===================== ======================================================== ====================
``component``         Component which caused this problem.                     ``'time'``
``hostname``          Hostname of the affected machine.                        ``'fiasco'``
``os_release``        Operating system release string.                         ``'Fedora release 17 (Beefy Miracle)'``
``uid``               User ID                                                  ``1000``
``username``                                                                   ``'jeff'``
``architecture``      Machine architecture string                              ``'x86_64'``
``kernel``            Kernel version string                                    ``'3.6.6-1.fc17.x86_64'``
``package``           Package string                                           ``'time-1.7-40.fc17.x86_64'``
``time``              Time of the occurrence (unixtime)                        ``datetime.datetime(2012, 12, 2, 16, 18, 41)``
``count``             Number of times this problem occurred                    ``1``
===================== ======================================================== ====================

Parsed package data is also available:

===================== ======================================================== ====================
Property              Meaning                                                  Example
===================== ======================================================== ====================
``pkg_name``          Package name                                             ``'time'``
``pkg_epoch``         Package epoch                                            ``0``
``pkg_version``       Package version                                          ``'1.7'``
``pkg_release``       Package release                                          ``'40.fc17'``
``pkg_arch``          Package architecture                                     ``'x86_64'``
===================== ======================================================== ====================

Other common properties (presence differs based on problem type):

===================== ======================================================== ====================================== ===============================
Property              Meaning                                                  Example                                Applicable
===================== ======================================================== ====================================== ===============================
``abrt_version``      ABRT version string                                      ``'2.0.18.84.g211c'``                  Crashes caught by ABRT
``cgroup``            cgroup (control group) information for crashed process   ``'9:perf_event:/\n8:blkio:/\n...'``   C/C++
``core_backtrace``    Machine readable backtrace with no private data                                                 C/C++, Python, Ruby, Kerneloops
``backtrace``         Original backtrace or backtrace produced by retracing                                           C/C++ (after retracing), Python, Ruby, Xorg, Kerneloops
                      process
``dso_list``          List of dynamic libraries loaded at the time of crash                                           C/C++, Python
``maps``              Copy of /proc/<pid>/maps file of the problem executable                                         C/C++
``cmdline``           Copy of /proc/<pid>/cmdline file                         ``'/usr/bin/gtk-builder-convert'``     C/C++, Python, Ruby, Kerneloops
``coredump``          Coredump of the crashing process                                                                C/C++
``environ``           Runtime environment of the process                                                              C/C++, Python
``open_fds``          List of file descriptors open at the time of crash                                              C/C++
``pid``               Process ID                                               ``'42'``                               C/C++, Python, Ruby
``proc_pid_status``   Copy of /proc/<pid>/status file                                                                 C/C++
``limits``            Copy of /proc/<pid>/limits file                                                                 C/C++
``var_log_messages``  Part of the /var/log/messages file which contains crash
                      information                                                                                     C/C++
``suspend_stats``     Copy of /sys/kernel/debug/suspend_stats                                                         Kerneloops
``reported_to``       If the problem was already reported, this item contains                                         Reported problems
                      URLs of the services where it was reported
``event_log``         ABRT event log                                                                                  Reported problems
``dmesg``             Copy of dmesg                                                                                   Kerneloops
===================== ======================================================== ====================================== ===============================
