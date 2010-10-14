/*
    normalize_glib.c

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
#include "normalize.h"
#include "frame.h"
#include "thread.h"
#include "utils.h"
#include <stdbool.h>
#include <string.h>

void
btp_normalize_glib_thread(struct btp_thread *thread)
{
    struct btp_frame *frame = thread->frames;
    while (frame)
    {
        struct btp_frame *next_frame = frame->next;

        /* Normalize frame names. */
        if (frame->function_name &&
            0 == strncmp(frame->function_name, "IA__g_", strlen("IA__g_")))
        {
            /* Remove the IA__ prefix. The strcpy function cannot be
             * used for that because the source and destination
             * pointers overlap. */
            char *p = frame->function_name;
            while ((*p = p[4]) != '\0')
                ++p;
        }

        /* Remove frames which are not a cause of the crash. */
        bool removable =
            btp_frame_calls_func_in_file2(frame, "g_log", "gmessages.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_logv", "gmessages.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_assertion_message", "gtestutils.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_assertion_message_expr", "gtestutils.c", "libglib");
        if (removable)
        {
            btp_thread_remove_frame(thread, frame);
        }

        frame = next_frame;
    }
}
