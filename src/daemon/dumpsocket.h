/*
  Copyright (C) 2010  ABRT team

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#ifndef ABRT_DUMPSOCKET_H
#define ABRT_DUMPSOCKET_H

/*
Unix socket in ABRT daemon for creating new dump directories.

Why to use socket for creating dump dirs? Security. When a Python
script throws unexpected exception, ABRT handler catches it, running
as a part of that broken Python application. The application is running
with certain SELinux privileges, for example it can not execute other
programs, or to create files in /var/cache or anything else required
to properly fill a dump directory. Adding these privileges to every
application would weaken the security.
The most suitable solution is for the Python application
to open a socket where ABRT daemon is listening, write all relevant
data to that socket, and close it. ABRT daemon handles the rest.

** Protocol

Initializing new dump:
open /var/run/abrt.socket

Providing dump data (hook writes to the socket):
-> "PID="
   number 0 - PID_MAX (/proc/sys/kernel/pid_max)
   \0
-> "EXECUTABLE="
   string (maximum length ~MAX_PATH)
   \0
-> "BACKTRACE="
   string (maximum length 1 MB)
   \0
-> "ANALYZER="
   string (maximum length 100 bytes)
   \0
-> "BASENAME="
   string (maximum length 100 bytes, no slashes)
   \0
-> "REASON="
   string (maximum length 512 bytes)
   \0

Finalizing dump creation:
-> "DONE"
   \0
*/

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the dump socket, usually in /var/run directory
 * (the path depends on compile-time configuration).
 */
extern void dumpsocket_init();

/* Releases all resources used by dumpsocket. */
extern void dumpsocket_shutdown();

#ifdef __cplusplus
}
#endif

#endif
