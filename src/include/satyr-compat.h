/*
    Copyright (C) 2013 RedHat inc.

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
/*
 * Compatibility header for transition from btparser to satyr. When the
 * possibility to compile abrt against btparser is removed, delete this file
 * and include the relevant satyr headers instead.
 */
#ifndef SATYR_COMPAT_H_
#define SATYR_COMPAT_H_

#include "config.h"

#ifdef USE_SATYR
/* Satyr is used, nothing to do, just include everything we may need. */

#include <satyr/core_frame.h>
#include <satyr/core_stacktrace.h>
#include <satyr/core_thread.h>
#include <satyr/distance.h>
#include <satyr/gdb_frame.h>
#include <satyr/gdb_stacktrace.h>
#include <satyr/location.h>
#include <satyr/metrics.h>
#include <satyr/normalize.h>
#include <satyr/sha1.h>
#include <satyr/utils.h>

#define BACKTRACE_DUP_THRESHOLD 0.3

#else /* USE_SATYR */
/* We use btparser. Wrap the calls to btparser in a satyr-compatible interface. */

#include <btparser/frame.h>
#include <btparser/thread.h>
#include <btparser/normalize.h>
#include <btparser/metrics.h>
#include <btparser/core-backtrace.h>
#include <btparser/backtrace.h>
#include <btparser/frame.h>
#include <btparser/location.h>
#include "libabrt.h" /* xstrdup */

#define BACKTRACE_DUP_THRESHOLD 2.0

/* abrt-handle-event.c */
#define sr_core_stacktrace btp_thread
#define sr_core_thread btp_thread

enum sr_distance_type
{
    SR_DISTANCE_DAMERAU_LEVENSHTEIN
};

/* The functions should be static but that generates unused function warnings.
 * We don't include this header in two compilation units that are then linked
 * together anyway, so this should work. And this file should be gone soon ...
 */
struct sr_core_stacktrace *
sr_core_stacktrace_from_json_text(const char *text,
                                  char **error_message)
{
    struct btp_thread *thread = btp_load_core_backtrace(text);
    if (!thread)
    {
        *error_message = xstrdup(
            "Failed to parse backtrace, considering it not duplicate");
        return NULL;
    }
    return btp_load_core_backtrace(text);
}

struct sr_core_thread *
sr_core_stacktrace_find_crash_thread(struct sr_core_stacktrace *stacktrace)
{
    return stacktrace;
}

int
sr_core_thread_get_frame_count(struct sr_core_thread *thread)
{
    return btp_thread_get_frame_count(thread);
}

float
sr_distance_core(enum sr_distance_type distance_type,
                 struct sr_core_thread *thread1,
                 struct sr_core_thread *thread2)
{
    return btp_thread_levenshtein_distance_custom(thread1, thread2, true,
        btp_core_backtrace_frame_cmp);
}

void
sr_core_stacktrace_free(struct sr_core_stacktrace *stacktrace)
{
    btp_free_core_backtrace(stacktrace);
}

/* abrt-action-analyze-backtrace.c */
#define sr_location btp_location
#define sr_gdb_stacktrace btp_backtrace
#define sr_gdb_frame btp_frame

void
sr_location_init(struct sr_location *location)
{
    btp_location_init(location);
}

struct sr_gdb_stacktrace *
sr_gdb_stacktrace_parse(const char **input,
                        struct sr_location *location)
{
    return btp_backtrace_parse(input, location);
}

char *
sr_gdb_stacktrace_get_duplication_hash(struct sr_gdb_stacktrace *stacktrace)
{
    return btp_backtrace_get_duplication_hash(stacktrace);
}

float
sr_gdb_stacktrace_quality_complex(struct sr_gdb_stacktrace *stacktrace)
{
    return btp_backtrace_quality_complex(stacktrace);
}

struct sr_gdb_frame *
sr_gdb_stacktrace_get_crash_frame(struct sr_gdb_stacktrace *stacktrace)
{
    return btp_backtrace_get_crash_frame(stacktrace);
}

void
sr_gdb_frame_free(struct sr_gdb_frame *frame)
{
    btp_frame_free(frame);
}

void
sr_gdb_stacktrace_free(struct sr_gdb_stacktrace *stacktrace)
{
    btp_backtrace_free(stacktrace);
}

#endif /* USE_SATYR */

#endif /* SATYR_COMPAT_H_ */
