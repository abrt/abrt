/*
    normalize.h

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
#ifndef BTPARSER_NORMALIZE_H
#define BTPARSER_NORMALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

struct btp_thread;
struct btp_backtrace;

void
btp_normalize_thread(struct btp_thread *thread);

void
btp_normalize_backtrace(struct btp_backtrace *backtrace);

/**
 */
void
btp_normalize_dbus_thread(struct btp_thread *thread);

void
btp_normalize_gdk_thread(struct btp_thread *thread);

void
btp_normalize_glib_thread(struct btp_thread *thread);

/**
 * Checks whether the thread it contains some function used to exit
 * application.  If a frame with the function is found, it is
 * returned.  If there are multiple frames with abort function, the
 * lowest one is returned.
 * @returns
 * Returns NULL if such a frame is not found.
 */
struct btp_frame *
btp_glibc_thread_find_exit_frame(struct btp_thread *thread);

void
btp_normalize_glibc_thread(struct btp_thread *thread);

void
btp_normalize_libstdcpp_thread(struct btp_thread *thread);

void
btp_normalize_linux_thread(struct btp_thread *thread);

void
btp_normalize_xorg_thread(struct btp_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
