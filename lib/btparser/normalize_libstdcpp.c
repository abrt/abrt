/*
    normalize_libstdcpp.c

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
#include <stdbool.h>

void
btp_normalize_libstdcpp_thread(struct btp_thread *thread)
{
    struct btp_frame *frame = thread->frames;
    while (frame)
    {
        struct btp_frame *next_frame = frame->next;

        /* Remove frames which are not a cause of the crash. */
        bool removable =
            btp_frame_calls_func_in_file(frame, "__gnu_cxx::__verbose_terminate_handler", "vterminate.cc") ||
            btp_frame_calls_func_in_file(frame, "__cxxabiv1::__terminate", "eh_terminate.cc") ||
            btp_frame_calls_func_in_file(frame, "std::terminate", "eh_terminate.cc") ||
            btp_frame_calls_func_in_file(frame, "__cxxabiv1::__cxa_throw", "eh_throw.cc");
        if (removable)
        {
            btp_thread_remove_frame(thread, frame);
        }

        frame = next_frame;
    }
}
