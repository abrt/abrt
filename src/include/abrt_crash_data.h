/*
    Copyright (C) 2009  Abrt team.
    Copyright (C) 2009  RedHat inc.

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
#ifndef ABRT_CRASH_DUMP_H_
#define ABRT_CRASH_DUMP_H_

#include "crash_data.h"
#include "abrt_types.h"

// Text bigger than this usually is attached, not added inline
#define CD_TEXT_ATT_SIZE (2*1024)

// Keep in sync with CCDump.py:

// Filenames in dump directory:
// filled by a hook:
#define FILENAME_ANALYZER     "analyzer"
#define FILENAME_EXECUTABLE   "executable"
#define FILENAME_BINARY       "binary"
#define FILENAME_CMDLINE      "cmdline"
#define FILENAME_REASON       "reason"
#define FILENAME_COREDUMP     "coredump"
#define FILENAME_BACKTRACE    "backtrace"
#define FILENAME_MAPS         "maps"
#define FILENAME_SMAPS        "smaps"
#define FILENAME_DUPHASH      "global_uuid" /* name is compat, to be renamed to "duphash" */
// Name of the function where the application crashed.
// Optional.
#define FILENAME_CRASH_FUNCTION "crash_function"
// filled by CDebugDump::Create() (which also fills FILENAME_UID):
#define FILENAME_ARCHITECTURE "architecture"
#define FILENAME_KERNEL       "kernel"
#define FILENAME_TIME         "time"
// From /etc/syste-release or /etc/redhat-release
#define FILENAME_OS_RELEASE   "os_release"
// Filled by <what?>
#define FILENAME_PACKAGE      "package"
#define FILENAME_COMPONENT    "component"
#define FILENAME_COMMENT      "comment"
#define FILENAME_RATING       "rating"
#define FILENAME_HOSTNAME     "hostname"
// Optional. Set to "1" by abrt-handle-upload for every unpacked crashdump
#define FILENAME_REMOTE       "remote"
#define FILENAME_TAINTED      "kernel_tainted"
// TODO: TicketUploader also has open-coded "TICKET", "CUSTOMER" files

#define FILENAME_UID       "uid"
#define FILENAME_UUID      "uuid"
#define FILENAME_INFORMALL "inform_all_users"
#define FILENAME_COUNT     "count"
#define FILENAME_MESSAGE   "message"
// Not stored as files, added "on the fly":
#define CD_DUMPDIR         "DumpDir"
// "Which events are possible (make sense) on this crash dump?"
// (a string with "\n" terminated event names)
#define CD_EVENTS          "Events"


#ifdef __cplusplus
extern "C" {
#endif

#define log_crash_data abrt_log_crash_data
void log_crash_data(crash_data_t *crash_data, const char *pfx);

#ifdef __cplusplus
}
#endif

#endif
