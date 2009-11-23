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
#include "backtrace.h"
#include <stdlib.h>

struct frame *frame_new()
{
  struct frame *f = malloc(sizeof(struct frame));
  if (!f)
  {
    puts("Error while allocating memory for backtrace frame.");
    exit(5);
  }

  f->function = NULL;
  f->number = 0;
  f->sourcefile = NULL;
  f->next = NULL;
  return f;
}

void frame_free(struct frame *f)
{
  if (f->function)
    free(f->function);
  if (f->sourcefile)
    free(f->sourcefile);
  free(f);
}

struct frame *frame_add_sibling(struct frame *a, struct frame *b)
{
  struct frame *aa = a;
  while (aa->next)
    aa = aa->next;

  aa->next = b;
  return a;
}

static void frame_print_tree(struct frame *frame)
{
  printf(" #%d", frame->number);
  if (frame->function)
    printf(" %s", frame->function);
  if (frame->sourcefile)
  {
    if (frame->function)
      printf(" at");
    printf(" %s", frame->sourcefile);
  }

  puts(""); /* newline */
}

struct thread *thread_new()
{
  struct thread *t = malloc(sizeof(struct thread));
  if (!t)
  {
    puts("Error while allocating memory for backtrace thread.");
    exit(5);
  }

  t->number = 0;
  t->frames = NULL;
  t->next = NULL;
}

void thread_free(struct thread *t)
{
  while (t->frames)
  {
    struct frame *rm = t->frames;
    t->frames = rm->next;
    frame_free(rm);
  }

  free(t);
}

struct thread *thread_add_sibling(struct thread *a, struct thread *b)
{
  struct thread *aa = a;
  while (aa->next)
    aa = aa->next;

  aa->next = b;
  return a;
}

static int thread_get_frame_count(struct thread *thread)
{
  struct frame *f = thread->frames;
  int count = 0;
  while (f)
  {
    f = f->next;
    ++count;
  }
  return count;
}

static void thread_print_tree(struct thread *thread)
{
  int framecount = thread_get_frame_count(thread);
  printf("Thread no. %d (%d frames)\n", thread->number, framecount);
  struct frame *frame = thread->frames;
  while (frame)
  {
    frame_print_tree(frame);
    frame = frame->next;
  }
}

struct backtrace *backtrace_new()
{
  struct backtrace *bt = malloc(sizeof(struct backtrace));
  if (!bt)
  {
    puts("Error while allocating memory for backtrace.");
    exit(5);
  }

  bt->threads = NULL;
  bt->crash = NULL;
}

void backtrace_free(struct backtrace *bt)
{
  while (bt->threads)
  {
    struct thread *rm = bt->threads;
    bt->threads = rm->next;
    thread_free(rm);
  }

  if (bt->crash)
    frame_free(bt->crash);

  free(bt);
}

static int backtrace_get_thread_count(struct backtrace *bt)
{
  struct thread *t = bt->threads;
  int count = 0;
  while (t)
  {
    t = t->next;
    ++count;
  }
  return count;
}

void backtrace_print_tree(struct backtrace *bt)
{
  printf("Thread count: %d\n", backtrace_get_thread_count(bt));
  if (bt->crash)
  {
    printf("Crash frame: ");
    frame_print_tree(bt->crash);
  }

  struct thread *thread = bt->threads;
  while (thread)
  {
    thread_print_tree(thread);
    thread = thread->next;
  }
}

/*
 * Checks whether the thread it contains some known "abort" function.
 * Nonrecursive.
 */
static bool thread_contain_abort_frame(struct thread *thread)
{
  int depth = 15; /* check only 15 top frames on the stack */
  struct frame *frame = thread->frames;
  while (frame && depth)
  {
    if (frame->function
	&& 0 == strcmp(frame->function, "raise") 
	&& frame->sourcefile 
	&& 0 == strcmp(frame->sourcefile, "../nptl/sysdeps/unix/sysv/linux/pt-raise.c"))
    {
      return true;
    }

    --depth;
    frame = frame->next;
  }

  return false;
}

/*
 * Loop through all threads and if a single one contains the crash frame on the top,
 * return it. Otherwise, return NULL.
 *
 * If require_abort is true, it is also required that the thread containing 
 * the crash frame contains some known "abort" function. In this case there can be
 * multiple threads with the crash frame on the top, but only one of them might
 * contain the abort function to succeed.
 */
static struct thread *backtrace_find_crash_thread_from_crash_frame(struct thread *first_thread,
								   struct frame *crash_frame,
								   bool require_abort)
{
  /*
   * This code can be extended to compare something else when the function
   * name is not available.
   */
  if (!first_thread || !crash_frame || !crash_frame->function)
    return NULL;

  struct thread *result = NULL;
  struct thread *thread = first_thread;
  while (thread)
  {
    if (thread->frames 
	&& thread->frames->function
	&& 0 == strcmp(thread->frames->funciton, backtrace->crash->function)
        && (!require_abort || thread_contain_abort_frame(thread)))
    {
      if (result == NULL)
	result = thread;
      else
      {
	/* Second frame with the same function. Failure. */
	return NULL;
      }
    }

    thread = thread->next;
  }
  
  return result;
}

struct thread *backtrace_find_crash_thread(struct backtrace *backtrace)
{
  /* If there is no thread, be silent and report NULL. */
  if (!backtrace->threads)
    return NULL;
  
  /* If there is just one thread, it is simple. */
  if (!backtrace->threads->next)
    return backtrace->threads;

  /* If we have a crash frame *and* there is just one thread which has 
   * this frame on the top, it is also simple. 
   */
  struct thread *thread;
  thread = backtrace_find_crash_thread_from_crash_frame(backtrace->threads,
							backtrace->crash, 
							false);
  if (thread)
    return thread;

  /* There are multiple threads with a frame indistinguishable from 
   * the crash frame on the top of stack.
   * Try to search for known abort functions.
   */
  thread = backtrace_find_crash_thread_from_crash_frame(backtrace->threads,
							backtrace->crash, 
							true);
  
  return thread; /* result or null */
}
