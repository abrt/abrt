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
  f->args = NULL;
  f->number = 0;
  f->binfile = NULL;
  f->sourcefile = NULL;
  f->crash = false;
  f->next = NULL;
  return f;
}

void frame_free(struct frame *f)
{
  if (f->function)
    free(f->function);
  if (f->args)
    free(f->args);
  if (f->binfile)
    free(f->binfile);
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

static void thread_print_tree(struct thread *thread)
{
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
}

void backtrace_free(struct backtrace *bt)
{
  while (bt->threads)
  {
    struct thread *rm = bt->threads;
    bt->threads = rm->next;
    thread_free(rm);
  }

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
  struct thread *thread = bt->threads;
  while (thread)
  {
    thread_print_tree(thread);
    thread = thread->next;
  }
}
