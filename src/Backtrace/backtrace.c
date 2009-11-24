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
#include <string.h>

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
  f->signal_handler_called = false;
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

static void frame_print_tree(struct frame *frame, bool verbose)
{
  if (verbose)
    printf(" #%d", frame->number);
  else
    printf(" ");

  if (frame->function)
    printf(" %s", frame->function);
  if (frame->sourcefile)
  {
    if (frame->function)
      printf(" at");
    printf(" %s", frame->sourcefile);
  }

  if (frame->signal_handler_called)
    printf(" <signal handler called>");

  puts(""); /* newline */
}

static bool frame_is_exit_handler(struct frame *frame)
{
  return (frame->function
	  && frame->sourcefile
	  && 0 == strcmp(frame->function, "__run_exit_handlers")
	  && NULL != strstr(frame->sourcefile, "exit.c"));
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

static void thread_print_tree(struct thread *thread, bool verbose)
{
  int framecount = thread_get_frame_count(thread);
  if (verbose)
    printf("Thread no. %d (%d frames)\n", thread->number, framecount);
  else
    printf("Thread\n");
  struct frame *frame = thread->frames;
  while (frame)
  {
    frame_print_tree(frame, verbose);
    frame = frame->next;
  }
}

/*
 * Checks whether the thread it contains some known "abort" function.
 * If a frame with the function is found, it is returned.
 * If there are multiple frames with abort function, the lowest 
 * one is returned.
 * Nonrecursive.
 */
static struct frame *thread_find_abort_frame(struct thread *thread)
{
  struct frame *frame = thread->frames;
  struct frame *result = NULL;
  while (frame)
  {
    if (frame->function && frame->sourcefile)
    {
      if (0 == strcmp(frame->function, "raise") 
	  && NULL != strstr(frame->sourcefile, "pt-raise.c"))
	result = frame;
      else if (0 == strcmp(frame->function, "exit")
	       && NULL != strstr(frame->sourcefile, "exit.c"))
	result = frame;
      else if (0 == strcmp(frame->function, "abort")
	       && NULL != strstr(frame->sourcefile, "abort.c"))
	result = frame;
      else if (frame_is_exit_handler(frame))
	result = frame;
    }
  
    frame = frame->next;
  }

  return result;
}

static void thread_remove_exit_handlers(struct thread *thread)
{
  struct frame *frame = thread->frames;
  while (frame)
  {
    if (frame_is_exit_handler(frame))
    {
      /* Delete all frames from the beginning to this frame. */
      while (thread->frames != frame)
      {
        struct frame *rm = thread->frames;
        thread->frames = thread->frames->next;
        frame_free(rm);
      }
      return;
    }

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

void backtrace_print_tree(struct backtrace *backtrace, bool verbose)
{
  if (verbose)
    printf("Thread count: %d\n", backtrace_get_thread_count(backtrace));

  if (backtrace->crash && verbose)
  {
    printf("Crash frame: ");
    frame_print_tree(backtrace->crash, verbose);
  }

  struct thread *thread = backtrace->threads;
  while (thread)
  {
    thread_print_tree(thread, verbose);
    thread = thread->next;
  }
}

void backtrace_remove_threads_except_one(struct backtrace *backtrace, 
					 struct thread *one)
{
  while (backtrace->threads)
  {
    struct thread *rm = backtrace->threads;
    backtrace->threads = rm->next;
    if (rm != one)
      thread_free(rm);
  }

  one->next = NULL;
  backtrace->threads = one;
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
static struct thread *backtrace_find_crash_thread_from_crash_frame(struct backtrace *backtrace,
								   bool require_abort)
{
  /*
   * This code can be extended to compare something else when the function
   * name is not available.
   */
  if (!backtrace->threads || !backtrace->crash || !backtrace->crash->function)
    return NULL;

  struct thread *result = NULL;
  struct thread *thread = backtrace->threads;
  while (thread)
  {
    if (thread->frames 
	&& thread->frames->function
	&& 0 == strcmp(thread->frames->function, backtrace->crash->function)
        && (!require_abort || thread_find_abort_frame(thread)))
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
  thread = backtrace_find_crash_thread_from_crash_frame(backtrace, false);
  if (thread)
    return thread;

  /* There are multiple threads with a frame indistinguishable from 
   * the crash frame on the top of stack.
   * Try to search for known abort functions.
   */
  thread = backtrace_find_crash_thread_from_crash_frame(backtrace, true);
  
  return thread; /* result or null */
}

void backtrace_limit_frame_depth(struct backtrace *backtrace, int depth)
{
  if (depth <= 0)
    return;

  struct thread *thread = backtrace->threads;
  while (thread)
  {
    struct frame *frame = thread_find_abort_frame(thread);
    if (frame)
      frame = frame->next; /* Start counting from the frame following the abort fr. */
    else
      frame = thread->frames; /* Start counting from the first frame. */

    /* Skip some frames to get the required stack depth. */
    int i = depth;
    struct frame *last_frame;
    while (frame && i)
    {
      last_frame = frame;
      frame = frame->next;
      --i;
    }

    /* Delete the remaining frames. */
    last_frame->next = NULL;
    while (frame)
    {
      struct frame *rm = frame;
      frame = frame->next;
      frame_free(rm);
    }

    thread = thread->next;
  }
}

void backtrace_remove_exit_handlers(struct backtrace *backtrace)
{
  struct thread *thread = backtrace->threads;
  while (thread)
  {
    thread_remove_exit_handlers(thread);
    thread = thread->next;
  }  
}

