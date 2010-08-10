/*
    Backtrace parsing and processing.

    If we transform analyzer plugins to separate applications one day,
    this functionality should be moved to CCpp analyzer, which will
    then easily provide what abrt-backtrace utility provides now. Currently
    the code is used by abrt-backtrace, so it is shared in the utils
    library.

    Copyright (C) 2009, 2010  RedHat inc.

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
#ifndef BACKTRACE_H
#define BACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>

struct frame
{
  /* Function name, or NULL. */
  char *function;
  /* Frame number. */
  int number;
  /* Name of the source file, or binary file, or NULL. */
  char *sourcefile;
  bool signal_handler_called;
  /* Sibling frame, or NULL if this is the last frame in a thread. */
  struct frame *next;
};

struct thread
{
  int number;
  struct frame *frames;
  /* Sibling thread, or NULL if this is the last thread in a backtrace. */
  struct thread *next;
};

struct backtrace
{
  struct thread *threads;
  /*
   * The frame where the crash happened according to GDB.
   * It might be that we can not tell to which thread this frame belongs,
   * because all threads end with mutually indistinguishable frames.
   */
  struct frame *crash;
};

extern struct frame *frame_new();
extern void frame_free(struct frame *f);
extern struct frame *frame_add_sibling(struct frame *a, struct frame *b);

extern struct thread *thread_new();
extern void thread_free(struct thread *t);
extern struct thread *thread_add_sibling(struct thread *a, struct thread *b);
extern struct frame *thread_find_abort_frame(struct thread *thread);

extern struct backtrace *backtrace_new();
extern void backtrace_free(struct backtrace *bt);

/* Prints how internal backtrace representation looks to stdout. */
extern void backtrace_print_tree(struct backtrace *backtrace, bool verbose);

/* Returns the backtrace tree string representation. */
extern struct strbuf *backtrace_tree_as_str(struct backtrace *backtrace, bool verbose);

/*
 * Frees all threads except the one provided as parameters.
 * It does not check whether one is a member of backtrace.
 * Caller must know that.
 */
extern void backtrace_remove_threads_except_one(struct backtrace *backtrace,
						struct thread *one);

/*
 * Search all threads and tries to find the one that caused the crash.
 * It might return NULL if the thread cannot be determined.
 */
extern struct thread *backtrace_find_crash_thread(struct backtrace *backtrace);

extern void backtrace_limit_frame_depth(struct backtrace *backtrace, int depth);

/*
 * Exit handlers are all stack frames above  __run_exit_handlers()
 */
extern void backtrace_remove_exit_handlers(struct backtrace *backtrace);

/*
 * Removes frames known as not causing crash, but that are often
 * a part of a backtrace.
 */
extern void backtrace_remove_noncrash_frames(struct backtrace *backtrace);

/* Parses the backtrace and stores it to a structure.
 * @returns
 *    Returns the backtrace struct representation, or NULL if the parser failed.
 *    Caller of this function is responsible for backtrace_free()ing the returned value.
 * Defined in backtrace_parser.y.
 */
extern struct backtrace *backtrace_parse(char *input, bool debug_parser, bool debug_scanner);

/* Reads the input file and calculates "independent" backtrace from it. "Independent" means
 * that the memory addresses that differ from run to run are removed from the backtrace, and
 * also variable names and values are removed.
 *
 * This function can be called when backtrace_parse() call fails. It provides a shorter
 * version of backtrace, with a chance that hash calculated from the returned value can be used
 * to detect duplicates. However, this kind of duplicate detection is very low-quality.
 * @returns
 *   The independent backtrace. Caller is responsible for calling
 *   strbuf_free() on it.
 */
extern struct strbuf *independent_backtrace(const char *input);

/* Get the quality of backtrace, as a number of "stars".
 * @returns
 *   Value 0 to 4.
 */
extern int backtrace_rate_old(const char *backtrace);

/* Evaluates the quality of the backtrace, meaning the ratio of frames
 * with function name fully known to all frames.
 * @returns
 *   A number between 0 and 1. 0 means the lowest quality,
 *   1 means full backtrace is known.
 */
extern float backtrace_quality(struct backtrace *backtrace);

#ifdef __cplusplus
}
#endif

#endif
