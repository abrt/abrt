/*
    normalize.c

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
#include "backtrace.h"
#include <string.h>

void
btp_normalize_thread(struct btp_thread *thread)
{
    btp_normalize_dbus_thread(thread);
    btp_normalize_gdk_thread(thread);
    btp_normalize_glib_thread(thread);
    btp_normalize_glibc_thread(thread);
    btp_normalize_libstdcpp_thread(thread);
    btp_normalize_linux_thread(thread);
    btp_normalize_xorg_thread(thread);

    /* If the first frame has address 0x0000 and its name is '??', it
     * is a dereferenced null, and we remove it. This frame is not
     * really invalid, and it affects backtrace quality rating. See
     * Red Hat Bugzilla bug #639038.
     * @code
     * #0  0x0000000000000000 in ?? ()
     * No symbol table info available.
     * #1  0x0000000000422648 in main (argc=1, argv=0x7fffa57cf0d8) at totem.c:242
     *       error = 0x0
     *       totem = 0xdee070 [TotemObject]
     * @endcode
     */
    if (thread->frames &&
        thread->frames->address == 0x0000 &&
        thread->frames->function_name &&
        0 == strcmp(thread->frames->function_name, "??"))
    {
        btp_thread_remove_frame(thread, thread->frames);
    }

    /* If the last frame has address 0x0000 and its name is '??',
     * remove it. This frame is not really invalid, and it affects
     * backtrace quality rating. See Red Hat Bugzilla bug #592523.
     * @code
     * #2  0x00007f4dcebbd62d in clone ()
     * at ../sysdeps/unix/sysv/linux/x86_64/clone.S:112
     * No locals.
     * #3  0x0000000000000000 in ?? ()
     * @endcode
     */
    struct btp_frame *last = thread->frames;
    while (last && last->next)
        last = last->next;
    if (last &&
        last->address == 0x0000 &&
        last->function_name &&
        0 == strcmp(last->function_name, "??"))
    {
        btp_thread_remove_frame(thread, last);
    }
}

void
btp_normalize_backtrace(struct btp_backtrace *backtrace)
{
    struct btp_thread *thread = backtrace->threads;
    while (thread)
    {
        btp_normalize_thread(thread);
        thread = thread->next;
    }
}
