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
#ifndef ABRT_PROBLEM_DATA_H_
#define ABRT_PROBLEM_DATA_H_

#include "libreport_problem_data.h"
#include "libreport_types.h"
#include "dump_dir.h"

// Text bigger than this usually is attached, not added inline
// was 2k, now bumbed up to 20k:
#define CD_TEXT_ATT_SIZE (20*1024)

// Filenames in dump directory:
// filled by a hook:
#define FILENAME_REASON       "reason"      /* mandatory */
#define FILENAME_UID          "uid"         /* mandatory */
#define FILENAME_TIME         "time"        /* mandatory */
#define FILENAME_ANALYZER     "analyzer"
#define FILENAME_EXECUTABLE   "executable"
#define FILENAME_BINARY       "binary"
#define FILENAME_CMDLINE      "cmdline"
#define FILENAME_COREDUMP     "coredump"
#define FILENAME_BACKTRACE    "backtrace"
#define FILENAME_MAPS         "maps"
#define FILENAME_SMAPS        "smaps"
#define FILENAME_ENVIRON      "environ"
#define FILENAME_DUPHASH      "duphash"
// Name of the function where the application crashed.
// Optional.
#define FILENAME_CRASH_FUNCTION "crash_function"
// filled by CDebugDump::Create() (which also fills FILENAME_UID):
#define FILENAME_ARCHITECTURE "architecture"
#define FILENAME_KERNEL       "kernel"
// From /etc/system-release or /etc/redhat-release
#define FILENAME_OS_RELEASE   "os_release"
// Filled by <what?>
#define FILENAME_PACKAGE      "package"
#define FILENAME_COMPONENT    "component"
#define FILENAME_COMMENT      "comment"
#define FILENAME_RATING       "backtrace_rating"
#define FILENAME_HOSTNAME     "hostname"
// Optional. Set to "1" by abrt-handle-upload for every unpacked dump
#define FILENAME_REMOTE       "remote"
#define FILENAME_TAINTED      "kernel_tainted"
#define FILENAME_TAINTED_SHORT "kernel_tainted_short"
#define FILENAME_TAINTED_LONG  "kernel_tainted_long"

#define FILENAME_UUID         "uuid"
#define FILENAME_COUNT        "count"
/* Multi-line list of places problem was reported.
 * Recommended line format:
 * "Reporter: VAR=VAL VAR=VAL"
 * Use add_reported_to(dd, "line_without_newline"): it adds line
 * only if it is not already there.
 */
#define FILENAME_REPORTED_TO  "reported_to"
#define FILENAME_EVENT_LOG    "event_log"

// Not stored as files, added "on the fly":
#define CD_DUMPDIR            "Directory"
//UNUSED:
//// "Which events are possible (make sense) on this dump dir?"
//// (a string with "\n" terminated event names)
//#define CD_EVENTS             "Events"

/* FILENAME_EVENT_LOG is trimmed to below LOW_WATERMARK
 * when it reaches HIGH_WATERMARK size
 */
enum {
    EVENT_LOG_HIGH_WATERMARK = 30 * 1024,
    EVENT_LOG_LOW_WATERMARK  = 20 * 1024,
};

#ifdef __cplusplus
extern "C" {
#endif

#define add_reported_to abrt_add_reported_to
void add_reported_to(struct dump_dir *dd, const char *line);

#define log_problem_data abrt_log_problem_data
void log_problem_data(problem_data_t *problem_data, const char *pfx);

#ifdef __cplusplus
}
#endif

#endif
