/*
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
#ifndef BACKTRACE_H
#define BACKTRACE_H

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

extern struct backtrace *backtrace_new();
extern void backtrace_free(struct backtrace *bt);

/* Prints how internal backtrace representation looks to stdout. */
extern void backtrace_print_tree(struct backtrace *backtrace, bool verbose);

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

/* Defined in parser.y. */
extern struct backtrace *do_parse(char *input, bool debug_parser, bool debug_scanner);

#endif
