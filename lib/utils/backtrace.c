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
#include "strbuf.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xfuncs.h"

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

/* Appends a string representation of 'frame' to the 'str'. */
static void frame_append_str(struct frame *frame, struct strbuf *str, bool verbose)
{
    if (verbose)
        strbuf_append_strf(str, " #%d", frame->number);
    else
        strbuf_append_str(str, " ");

    if (frame->function)
        strbuf_append_strf(str, " %s", frame->function);
    if (verbose && frame->sourcefile)
    {
        if (frame->function)
            strbuf_append_str(str, " at");
        strbuf_append_strf(str, " %s", frame->sourcefile);
    }

    if (frame->signal_handler_called)
        strbuf_append_str(str, " <signal handler called>");

    strbuf_append_str(str, "\n");
}

static bool frame_is_exit_handler(struct frame *frame)
{
  return (frame->function
	  && frame->sourcefile
	  && 0 == strcmp(frame->function, "__run_exit_handlers")
	  && NULL != strstr(frame->sourcefile, "exit.c"));
}

/* Checks if a frame contains abort function used
 * by operating system to exit application.
 * E.g. in C it's called "abort" or "raise".
 */
static bool frame_is_abort_frame(struct frame *frame)
{
  if (!frame->function || !frame->sourcefile)
    return false;

  if (0 == strcmp(frame->function, "raise")
      && (NULL != strstr(frame->sourcefile, "pt-raise.c")
          || NULL != strstr(frame->sourcefile, "/libc.so.6")))
    return true;
  else if (0 == strcmp(frame->function, "exit")
           && NULL != strstr(frame->sourcefile, "exit.c"))
    return true;
  else if (0 == strcmp(frame->function, "abort")
           && (NULL != strstr(frame->sourcefile, "abort.c")
               || NULL != strstr(frame->sourcefile, "/libc.so.6")))
    return true;
  else if (frame_is_exit_handler(frame))
    return true;

  return false;
}

static bool frame_is_noncrash_frame(struct frame *frame)
{
  /* Abort frames. */
  if (frame_is_abort_frame(frame))
    return true;

  if (!frame->function)
    return false;

  if (0 == strcmp(frame->function, "__kernel_vsyscall"))
    return true;

  if (0 == strcmp(frame->function, "__assert_fail"))
    return true;

  if (!frame->sourcefile)
    return false;

  /* GDK */
  if (0 == strcmp(frame->function, "gdk_x_error")
      && 0 == strcmp(frame->sourcefile, "gdkmain-x11.c"))
    return true;

  /* X.org */
  if (0 == strcmp(frame->function, "_XReply")
      && 0 == strcmp(frame->sourcefile, "xcb_io.c"))
    return true;
  if (0 == strcmp(frame->function, "_XError")
      && 0 == strcmp(frame->sourcefile, "XlibInt.c"))
    return true;
  if (0 == strcmp(frame->function, "XSync")
      && 0 == strcmp(frame->sourcefile, "Sync.c"))
    return true;
  if (0 == strcmp(frame->function, "process_responses")
      && 0 == strcmp(frame->sourcefile, "xcb_io.c"))
    return true;

  /* glib */
  if (0 == strcmp(frame->function, "IA__g_log")
      && 0 == strcmp(frame->sourcefile, "gmessages.c"))
    return true;
  if (0 == strcmp(frame->function, "IA__g_logv")
      && 0 == strcmp(frame->sourcefile, "gmessages.c"))
    return true;
  if (0 == strcmp(frame->function, "IA__g_assertion_message")
      && 0 == strcmp(frame->sourcefile, "gtestutils.c"))
    return true;
  if (0 == strcmp(frame->function, "IA__g_assertion_message_expr")
      && 0 == strcmp(frame->sourcefile, "gtestutils.c"))
    return true;

  /* DBus */
  if (0 == strcmp(frame->function, "gerror_to_dbus_error_message")
      && 0 == strcmp(frame->sourcefile, "dbus-gobject.c"))
    return true;
  if (0 == strcmp(frame->function, "dbus_g_method_return_error")
      && 0 == strcmp(frame->sourcefile, "dbus-gobject.c"))
    return true;

  /* libstdc++ */
  if (0 == strcmp(frame->function, "__gnu_cxx::__verbose_terminate_handler")
      && NULL != strstr(frame->sourcefile, "/vterminate.cc"))
    return true;
  if (0 == strcmp(frame->function, "__cxxabiv1::__terminate")
      && NULL != strstr(frame->sourcefile, "/eh_terminate.cc"))
    return true;
  if (0 == strcmp(frame->function, "std::terminate")
      && NULL != strstr(frame->sourcefile, "/eh_terminate.cc"))
    return true;
  if (0 == strcmp(frame->function, "__cxxabiv1::__cxa_throw")
      && NULL != strstr(frame->sourcefile, "/eh_throw.cc"))
    return true;

  return false;
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
  return t;
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

/* Appends string representation of 'thread' to the 'str'. */
static void thread_append_str(struct thread *thread, struct strbuf *str, bool verbose)
{
    int framecount = thread_get_frame_count(thread);
    if (verbose)
        strbuf_append_strf(str, "Thread no. %d (%d frames)\n", thread->number, framecount);
    else
        strbuf_append_str(str, "Thread\n");
    struct frame *frame = thread->frames;
    while (frame)
    {
        frame_append_str(frame, str, verbose);
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
struct frame *thread_find_abort_frame(struct thread *thread)
{
  struct frame *frame = thread->frames;
  struct frame *result = NULL;
  while (frame)
  {
    if (frame_is_abort_frame(frame))
      result = frame;

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

static void thread_remove_noncrash_frames(struct thread *thread)
{
  struct frame *prev = NULL;
  struct frame *cur = thread->frames;
  while (cur)
  {
    if (frame_is_noncrash_frame(cur))
    {
      /* This frame must be skipped, because it will
         be deleted. */
      if (prev)
	prev->next = cur->next;
      else
	thread->frames = cur->next;

      frame_free(cur);

      /* Set cur to be valid, as it will be used to
         advance to next item. */
      if (prev)
	cur = prev;
      else
      {
	cur = thread->frames;
        continue;
      }
    }

    prev = cur;
    cur = cur->next;
  }
}

/* Counts the number of quality frames and the number of all frames
 * in a thread.
 * @param ok_count
 * @param all_count
 *   Not zeroed. This function just adds the numbers to
 *   ok_count and all_count.
 */
static void thread_rating(struct thread *thread, int *ok_count, int *all_count)
{
    struct frame *frame = thread->frames;
    while (frame)
    {
        *all_count += 1;
        if (frame->signal_handler_called ||
            (frame->function && 0 != strcmp(frame->function, "??")))
        {
            *ok_count += 1;
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
  return bt;
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
    struct strbuf *strbuf = backtrace_tree_as_str(backtrace, verbose);
    puts(strbuf->buf);
    strbuf_free(strbuf);
}

struct strbuf *backtrace_tree_as_str(struct backtrace *backtrace, bool verbose)
{
    struct strbuf *str = strbuf_new();
    if (verbose)
        strbuf_append_strf(str, "Thread count: %d\n", backtrace_get_thread_count(backtrace));

    if (backtrace->crash && verbose)
    {
        strbuf_append_str(str, "Crash frame: ");
        frame_append_str(backtrace->crash, str, verbose);
    }

    struct thread *thread = backtrace->threads;
    while (thread)
    {
        thread_append_str(thread, str, verbose);
        thread = thread->next;
    }

    return str;
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
    struct frame *last_frame = NULL;
    while (frame && i)
    {
      last_frame = frame;
      frame = frame->next;
      --i;
    }

    /* Delete the remaining frames. */
    if (last_frame)
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

void backtrace_remove_noncrash_frames(struct backtrace *backtrace)
{
  struct thread *thread = backtrace->threads;
  while (thread)
  {
    thread_remove_noncrash_frames(thread);
    thread = thread->next;
  }
}

/* Belongs to independent_backtrace(). */
struct header
{
  struct strbuf *text;
  struct header *next;
};

/* Belongs to independent_backtrace(). */
static struct header *header_new()
{
  struct header *head = malloc(sizeof(struct header));
  if (!head)
  {
    puts("Error while allocating memory for backtrace header.");
    exit(5);
  }
  head->text = NULL;
  head->next = NULL;
  return head;
}

/* Recursively frees siblings. */
/* Belongs to independent_backtrace(). */
static void header_free(struct header *head)
{
  if (head->text)
    strbuf_free(head->text);
  if (head->next)
    header_free(head->next);
  free(head);
}

/* Inserts new header to array if it is not already there. */
/* Belongs to independent_backtrace(). */
static void header_set_insert(struct header *cur, struct strbuf *new)
{
  /* Duplicate found case. */
  if (strcmp(cur->text->buf, new->buf) == 0)
    return;

  /* Last item case, insert new header here. */
  if (cur->next == NULL)
  {
    cur->next = header_new();
    cur->next->text = new;
    return;
  }

  /* Move to next item in array case. */
  header_set_insert(cur->next, new);
}

struct strbuf *independent_backtrace(const char *input)
{
  struct strbuf *header = strbuf_new();
  bool in_bracket = false;
  bool in_quote = false;
  bool in_header = false;
  bool in_digit = false;
  bool has_at = false;
  bool has_filename = false;
  bool has_bracket = false;
  struct header *headers = NULL;

  const char *bk = input;
  while (*bk)
  {
    if (bk[0] == '#'
	&& bk[1] >= '0' && bk[1] <= '7'
	&& bk[2] == ' ' /* take only #0...#7 (8 last stack frames) */
	&& !in_quote)
    {
      if (in_header && !has_filename)
	strbuf_clear(header);
      in_header = true;
    }

    if (!in_header)
    {
      ++bk;
      continue;
    }

    if (isdigit(*bk) && !in_quote && !has_at)
      in_digit = true;
    else if (bk[0] == '\\' && bk[1] == '\"')
      bk++;
    else if (*bk == '\"')
      in_quote = in_quote == true ? false : true;
    else if (*bk == '(' && !in_quote)
    {
      in_bracket = true;
      in_digit = false;
      strbuf_append_char(header, '(');
    }
    else if (*bk == ')' && !in_quote)
    {
      in_bracket = false;
      has_bracket = true;
      in_digit = false;
      strbuf_append_char(header, '(');
    }
    else if (*bk == '\n' && has_filename)
    {
      if (headers == NULL)
      {
	headers = header_new();
	headers->text = header;
      }
      else
	header_set_insert(headers, header);

      header = strbuf_new();
      in_bracket = false;
      in_quote = false;
      in_header = false;
      in_digit = false;
      has_at = false;
      has_filename = false;
      has_bracket = false;
    }
    else if (*bk == ',' && !in_quote)
      in_digit = false;
    else if (isspace(*bk) && !in_quote)
      in_digit = false;
    else if (bk[0] == 'a' && bk[1] == 't' && has_bracket && !in_quote)
    {
      has_at = true;
      strbuf_append_char(header, 'a');
    }
    else if (bk[0] == ':' && has_at && isdigit(bk[1]) && !in_quote)
      has_filename = true;
    else if (in_header && !in_digit && !in_quote && !in_bracket)
      strbuf_append_char(header, *bk);

    bk++;
  }

  strbuf_free(header);

  struct strbuf *result = strbuf_new();
  struct header *loop = headers;
  while (loop)
  {
    strbuf_append_str(result, loop->text->buf);
    strbuf_append_char(result, '\n');
    loop = loop->next;
  }

  if (headers)
    header_free(headers); /* recursive */

  return result;
}

/* Belongs to backtrace_rate_old(). */
enum line_rating
{
    // RATING              EXAMPLE
    MissingEverything = 0, // #0 0x0000dead in ?? ()
    MissingFunction   = 1, // #0 0x0000dead in ?? () from /usr/lib/libfoobar.so.4
    MissingLibrary    = 2, // #0 0x0000dead in foobar()
    MissingSourceFile = 3, // #0 0x0000dead in FooBar::FooBar () from /usr/lib/libfoobar.so.4
    Good              = 4, // #0 0x0000dead in FooBar::crash (this=0x0) at /home/user/foobar.cpp:204
    BestRating = Good,
};

/* Belongs to backtrace_rate_old(). */
static enum line_rating rate_line(const char *line)
{
#define FOUND(x) (strstr(line, x) != NULL)
    /* see the comments at enum line_rating for possible combinations */
    if (FOUND(" at "))
        return Good;
    const char *function = strstr(line, " in ");
    if (function && function[4] == '?') /* " in ??" does not count */
        function = NULL;
    bool library = FOUND(" from ");
    if (function && library)
        return MissingSourceFile;
    if (function)
        return MissingLibrary;
    if (library)
        return MissingFunction;

    return MissingEverything;
#undef FOUND
}

/* just a fallback function, to be removed one day */
int backtrace_rate_old(const char *backtrace)
{
    int i, len;
    int multiplier = 0;
    int rating = 0;
    int best_possible_rating = 0;
    char last_lvl = 0;

    /* We look at the frames in reversed order, since:
     * - rate_line() checks starting from the first line of the frame
     * (note: it may need to look at more than one line!)
     * - we increase weight (multiplier) for every frame,
     *   so that topmost frames end up most important
     */
    len = 0;
    for (i = strlen(backtrace) - 1; i >= 0; i--)
    {
        if (backtrace[i] == '#'
            && (backtrace[i+1] >= '0' && backtrace[i+1] <= '9') /* #N */
            && (i == 0 || backtrace[i-1] == '\n')) /* it's at line start */
        {
            /* For one, "#0 xxx" always repeats, skip repeats */
            if (backtrace[i+1] == last_lvl)
                continue;
            last_lvl = backtrace[i+1];

            char *s = xstrndup(backtrace + i + 1, len);
            /* Replace tabs with spaces, rate_line() does not expect tabs.
             * Actually, even newlines may be there. Example of multiline frame
             * where " at SRCFILE" is on 2nd line:
             * #3  0x0040b35d in __libc_message (do_abort=<value optimized out>,
             *     fmt=<value optimized out>) at ../sysdeps/unix/sysv/linux/libc_fatal.c:186
             */
            char *p;
            for (p = s; *p; p++)
            {
                if (*p == '\t' || *p == '\n')
                    *p = ' ';
            }
            int lrate = rate_line(s);
            multiplier++;
            rating += lrate * multiplier;
            best_possible_rating += BestRating * multiplier;
            //log("lrate:%d rating:%d best_possible_rating:%d s:'%-.40s'",
            //    lrate, rating, best_possible_rating, s);
            free(s);
            len = 0; /* starting new line */
        }
        else
        {
            len++;
        }
    }

    /* Bogus 'backtrace' with zero frames? */
    if (best_possible_rating == 0)
        return 0;

    /* Returning number of "stars" to show */
    if (rating*10 >= best_possible_rating*8) /* >= 0.8 */
        return 4;
    if (rating*10 >= best_possible_rating*6)
        return 3;
    if (rating*10 >= best_possible_rating*4)
        return 2;
    if (rating*10 >= best_possible_rating*2)
        return 1;

    return 0;
}

float backtrace_quality(struct backtrace *backtrace)
{
    int ok_count = 0;
    int all_count = 0;
    struct thread *thread = backtrace->threads;
    while (thread)
    {
        thread_rating(thread, &ok_count, &all_count);
        thread = thread->next;
    }

    if (all_count == 0)
        return 0;
    return ok_count / (float)all_count;
}
