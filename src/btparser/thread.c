/*
    thread.c

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
#include "thread.h"
#include "frame.h"
#include "strbuf.h"
#include "utils.h"
#include "location.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct btp_thread *
btp_thread_new()
{
    struct btp_thread *thread = btp_malloc(sizeof(struct btp_thread));
    btp_thread_init(thread);
    return thread;
}

void
btp_thread_init(struct btp_thread *thread)
{
    thread->number = -1;
    thread->frames = NULL;
    thread->next = NULL;
}

void
btp_thread_free(struct btp_thread *thread)
{
    if (!thread)
        return;

    while (thread->frames)
    {
        struct btp_frame *rm = thread->frames;
        thread->frames = rm->next;
        btp_frame_free(rm);
    }

    free(thread);
}

struct btp_thread *
btp_thread_dup(struct btp_thread *thread, bool siblings)
{
    struct btp_thread *result = btp_thread_new();
    memcpy(result, thread, sizeof(struct btp_thread));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_thread_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    result->frames = btp_frame_dup(result->frames, true);

    return result;
}

int
btp_thread_cmp(struct btp_thread *t1, struct btp_thread *t2)
{
    int number = t1->number - t2->number;
    if (number != 0)
        return number;
    struct btp_frame *f1 = t1->frames, *f2 = t2->frames;
    do {
        if (f1 && !f2)
            return 1;
        else if (f2 && !f1)
            return -1;
        else if (f1 && f2)
        {
            int frames = btp_frame_cmp(f1, f2, true);
            if (frames != 0)
                return frames;
            f1 = f1->next;
            f2 = f2->next;
        }
    } while (f1 || f2);

    return 0;
}

struct btp_thread *
btp_thread_add_sibling(struct btp_thread *a, struct btp_thread *b)
{
    struct btp_thread *aa = a;
    while (aa->next)
        aa = aa->next;

    aa->next = b;
    return a;
}

int
btp_thread_get_frame_count(struct btp_thread *thread)
{
    struct btp_frame *frame = thread->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }
    return count;
}

void
btp_thread_quality_counts(struct btp_thread *thread,
                          int *ok_count,
                          int *all_count)
{
    struct btp_frame *frame = thread->frames;
    while (frame)
    {
        *all_count += 1;
        if (frame->signal_handler_called ||
            (frame->function_name
             && 0 != strcmp(frame->function_name, "??")))
        {
            *ok_count += 1;
        }
        frame = frame->next;
    }
}

float
btp_thread_quality(struct btp_thread *thread)
{
    int ok_count = 0, all_count = 0;
    btp_thread_quality_counts(thread, &ok_count, &all_count);
    if (0 == all_count)
        return 1;
    return ok_count/(float)all_count;
}

bool
btp_thread_remove_frame(struct btp_thread *thread,
                        struct btp_frame *frame)
{
    struct btp_frame *loop_frame = thread->frames, *prev_frame = NULL;
    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                thread->frames = loop_frame->next;

            btp_frame_free(loop_frame);
            return true;
        }
        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }
    return false;
}

bool
btp_thread_remove_frames_above(struct btp_thread *thread,
                               struct btp_frame *frame)
{
    /* Check that the frame is present in the thread. */
    struct btp_frame *loop_frame = thread->frames;
    while (loop_frame)
    {
        if (loop_frame == frame)
            break;
        loop_frame = loop_frame->next;
    }

    if (!loop_frame)
        return false;

    /* Delete all the frames up to the frame. */
    while (thread->frames != frame)
    {
        loop_frame = thread->frames->next;
        btp_frame_free(thread->frames);
        thread->frames = loop_frame;
    }

    return true;
}

void
btp_thread_remove_frames_below_n(struct btp_thread *thread,
                                 int n)
{
    assert(n >= 0);

    /* Skip some frames to get the required stack depth. */
    int i = n;
    struct btp_frame *frame = thread->frames, *last_frame = NULL;
    while (frame && i)
    {
        last_frame = frame;
        frame = frame->next;
        --i;
    }

    /* Delete the remaining frames. */
    if (last_frame)
        last_frame->next = NULL;
    else
        thread->frames = NULL;

    while (frame)
    {
        struct btp_frame *delete_frame = frame;
        frame = frame->next;
        btp_frame_free(delete_frame);
    }
}

void
btp_thread_append_to_str(struct btp_thread *thread,
                         struct strbuf *str,
                         bool verbose)
{
    int framecount = btp_thread_get_frame_count(thread);
    if (verbose)
    {
        strbuf_append_strf(str, "Thread no. %d (%d frames)\n",
                               thread->number, framecount);
    }
    else
        strbuf_append_str(str, "Thread\n");

    struct btp_frame *frame = thread->frames;
    while (frame)
    {
        btp_frame_append_to_str(frame, str, verbose);
        frame = frame->next;
    }
}

struct btp_thread *
btp_thread_parse(char **input,
                 struct btp_location *location)
{
    char *local_input = *input;

    /* Read the Thread keyword, which is mandatory. */
    int chars = btp_skip_string(&local_input, "Thread");
    location->column += chars;
    if (0 == chars)
    {
        location->message = "\"Thread\" header expected";
        return NULL;
    }

    /* Skip spaces, at least one space is mandatory. */
    int spaces = btp_skip_char_sequence(&local_input, ' ');
    location->column += spaces;
    if (0 == spaces)
    {
        location->message = "Space expected after the \"Thread\" keyword.";
        return NULL;
    }

    /* Read thread number. */
    struct btp_thread *imthread = btp_thread_new();
    int digits = btp_parse_unsigned_integer(&local_input, &imthread->number);
    location->column += digits;
    if (0 == digits)
    {
        location->message = "Thread number expected.";
        btp_thread_free(imthread);
        return NULL;
    }

    /* Skip spaces after the thread number and before parentheses. */
    spaces = btp_skip_char_sequence(&local_input, ' ');
    location->column += spaces;
    if (0 == spaces)
    {
        location->message = "Space expected after the thread number.";
        btp_thread_free(imthread);
        return NULL;
    }

    /* Read the LWP section in parentheses, optional. */
    location->column += btp_thread_skip_lwp(&local_input);

    /* Read the Thread keyword in parentheses, optional. */
    chars = btp_skip_string(&local_input, "(Thread ");
    location->column += chars;
    if (0 != chars)
    {
        /* Read the thread identification number. It can be either in
         * decimal or hexadecimal form.
         * Examples:
         * "Thread 10 (Thread 2476):"
         * "Thread 8 (Thread 0xb07fdb70 (LWP 6357)):"
         */
        digits = btp_skip_hexadecimal_number(&local_input);
        if (0 == digits)
            digits = btp_skip_unsigned_integer(&local_input);
        location->column += digits;
        if (0 == digits)
        {
            location->message = "The thread identification number expected.";
            btp_thread_free(imthread);
            return NULL;
        }

        /* Handle the optional " (LWP [0-9]+)" section. */
        location->column += btp_skip_char_sequence(&local_input, ' ');
        location->column += btp_thread_skip_lwp(&local_input);

        /* Read the end of the parenthesis. */
        if (!btp_skip_char(&local_input, ')'))
        {
            location->message = "Closing parenthesis for Thread expected.";
            btp_thread_free(imthread);
            return NULL;
        }
    }

    /* Read the end of the header line. */
    chars = btp_skip_string(&local_input, ":\n");
    if (0 == chars)
    {
        location->message = "Expected a colon followed by a newline ':\\n'.";
        btp_thread_free(imthread);
        return NULL;
    }
    /* Add the newline from the last btp_skip_string. */
    btp_location_add(location, 2, 0);

    /* Read the frames. */
    struct btp_frame *frame, *prevframe = NULL;
    struct btp_location frame_location;
    btp_location_init(&frame_location);
    while ((frame = btp_frame_parse(&local_input, &frame_location)))
    {
        if (prevframe)
        {
            btp_frame_add_sibling(prevframe, frame);
            prevframe = frame;
        }
        else
            imthread->frames = prevframe = frame;

        btp_location_add(location,
                         frame_location.line,
                         frame_location.column);
    }
    if (!imthread->frames)
    {
        location->message = frame_location.message;
        btp_thread_free(imthread);
        return NULL;
    }

    *input = local_input;
    return imthread;
}

int
btp_thread_skip_lwp(char **input)
{
    char *local_input = *input;
    int count = btp_skip_string(&local_input, "(LWP ");
    if (0 == count)
        return 0;
    int digits = btp_skip_unsigned_integer(&local_input);
    if (0 == digits)
        return 0;
    count += digits;
    if (!btp_skip_char(&local_input, ')'))
        return 0;
    *input = local_input;
    return count + 1;
}
