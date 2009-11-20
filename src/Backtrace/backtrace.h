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
  /* Arguments of the function call, or NULL. */
  char *args;
  /* Frame number. */
  int number;
  /* ?? */
  char *binfile;
  /* Name of the source file, or NULL. */
  char *sourcefile;
  /* True if this is the frame where the crash happened.  */ 
  bool crash;
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
extern void backtrace_print_tree(struct backtrace *bt);

/* Defined in parser.y. */
extern struct backtrace *do_parse(FILE *input, bool debug_parser, bool debug_scanner);

#endif
