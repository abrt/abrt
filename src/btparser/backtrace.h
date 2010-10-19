/*
    backtrace.h

    Copyright (C) 2010  Red Hat, Inc.

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
#ifndef BTPARSER_BACKTRACE_H
#define BTPARSER_BACKTRACE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct btp_thread;
struct btp_frame;
struct btp_location;

/**
 * A backtrace obtained at the time of a program crash, consisting of
 * several threads which contains frames.
 */
struct btp_backtrace
{
    struct btp_thread *threads;
    /**
     * The frame where the crash happened according to debugger.  It
     * might be that we can not tell to which thread this frame
     * belongs, because some threads end with mutually
     * indistinguishable frames.
     */
    struct btp_frame *crash;
};

/**
 * Creates and initializes a new backtrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_backtrace_free().
 */
struct btp_backtrace *
btp_backtrace_new();

/**
 * Initializes all members of the backtrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a backtrace structure placed on the
 * stack.
 */
void
btp_backtrace_init(struct btp_backtrace *backtrace);

/**
 * Releases the memory held by the backtrace, its threads and frames.
 * @param backtrace
 * If the backtrace is NULL, no operation is performed.
 */
void
btp_backtrace_free(struct btp_backtrace *backtrace);

/**
 * Creates a duplicate of the backtrace.
 * @param backtrace
 * The backtrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL. If the returned duplicate is not
 * shallow, it must be released by calling the function
 * btp_backtrace_free().
 */
struct btp_backtrace *
btp_backtrace_dup(struct btp_backtrace *backtrace);

/**
 * Returns a number of threads in the backtrace.
 * @param backtrace
 * It's not modified by calling this function.
 */
int
btp_backtrace_get_thread_count(struct btp_backtrace *backtrace);

/**
 * Removes all threads from the backtrace and deletes them, except the
 * one provided as a parameter.
 * @param thread
 * This function does not check whether the thread is a member of the backtrace.
 * If it's not, all threads are removed from the backtrace and then deleted.
 */
void
btp_backtrace_remove_threads_except_one(struct btp_backtrace *backtrace,
					struct btp_thread *thread);

/**
 * Search all threads and tries to find the one that caused the crash.
 * It might return NULL if the thread cannot be determined.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 */
struct btp_thread *
btp_backtrace_find_crash_thread(struct btp_backtrace *backtrace);

/**
 * Remove frames from the bottom of threads in the backtrace, until
 * all threads have at most 'depth' frames.
 * @param backtrace
 * Must be non-NULL pointer.
 */
void
btp_backtrace_limit_frame_depth(struct btp_backtrace *backtrace,
				int depth);

/**
 * Evaluates the quality of the backtrace. The quality is the ratio of
 * the number of frames with function name fully known to the number
 * of all frames.  This function does not take into account that some
 * frames are more important than others.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * backtrace is known.
 */
float
btp_backtrace_quality_simple(struct btp_backtrace *backtrace);

/**
 * Evaluates the quality of the backtrace. The quality is determined
 * depending on the ratio of frames with function name fully known to
 * all frames.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * backtrace is known.  The returned value takes into account that the
 * thread which caused the crash is more important than the other
 * threads, and the frames around the crash frame are more important
 * than distant frames.
 */
float
btp_backtrace_quality_complex(struct btp_backtrace *backtrace);

/**
 * Returns textual representation of the backtrace.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * This function never returns NULL. The caller is responsible for
 * releasing the returned memory using function free().
 */
char *
btp_backtrace_to_text(struct btp_backtrace *backtrace,
                      bool verbose);

/**
 * Analyzes the backtrace to get the frame where a crash occurred.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * The returned value must be released by calling btp_frame_free(),
 * when it's no longer needed.  NULL is returned if the crash frame is
 * not found.
 */
struct btp_frame *
btp_backtrace_get_crash_frame(struct btp_backtrace *backtrace);

/**
 * Calculates the duplication hash string of the backtrace.
 * @param backtrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * This function never returns NULL. The caller is responsible for
 * releasing the returned memory using function free().
 */
char *
btp_backtrace_get_duplication_hash(struct btp_backtrace *backtrace);

/**
 * Parses a textual backtrace and puts it into a structure.  If
 * parsing fails, the input parameter is not changed and NULL is
 * returned.
 * @code
 * struct btp_location location;
 * btp_location_init(&location);
 * char *input = "...";
 * struct btp_backtrace *backtrace = btp_backtrace_parse(input, location;
 * if (!backtrace)
 * {
 *   fprintf(stderr,
 *           "Failed to parse the backtrace.\n"
 *           "Line %d, column %d: %s\n",
 *           location.line,
 *           location.column,
 *           location.message);
 *   exit(-1);
 * }
 * btp_backtrace_free(backtrace);
 * @endcode
 * @param input
 * Pointer to the string with the backtrace. If this function returns
 * true, this pointer is modified to point after the backtrace that
 * was just parsed.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 * @returns
 * A newly allocated backtrace structure or NULL. A backtrace struct
 * is returned when at least one thread was parsed from the input and
 * no error occurred. The returned structure should be released by
 * btp_backtrace_free().
 */
struct btp_backtrace *
btp_backtrace_parse(char **input,
                    struct btp_location *location);

/**
 * Parse backtrace header if it is available in the backtrace.  The
 * header usually contains frame where the program crashed.
 * @param input
 * Pointer moved to point behind the header if the header is
 * successfully detected and parsed.
 * @param frame
 * If this function succeeds and returns true, *frame contains the
 * crash frame that is usually a part of the header. If no frame is
 * detected in the header, *frame is set to NULL.
 * @code
 * [New Thread 11919]
 * [New Thread 11917]
 * Core was generated by `evince file:///tmp/Factura04-05-2010.pdf'.
 * Program terminated with signal 8, Arithmetic exception.
 * #0  0x000000322a2362b9 in repeat (image=<value optimized out>,
 *     mask=<value optimized out>, mask_bits=<value optimized out>)
 *     at pixman-bits-image.c:145
 * 145     pixman-bits-image.c: No such file or directory.
 *         in pixman-bits-image.c
 * @endcode
 */
bool
btp_backtrace_parse_header(char **input,
                           struct btp_frame **frame,
                           struct btp_location *location);

#ifdef __cplusplus
}
#endif

#endif
