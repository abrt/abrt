/*
    Copyright (C) 2013  RedHat inc.

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

#ifndef _ABRT_INOTIFY_H_
#define _ABRT_INOTIFY_H_

#include <sys/inotify.h>

struct abrt_inotify_watch;

typedef void (* abrt_inotify_watch_handler)(
        struct abrt_inotify_watch *watch,
        struct inotify_event *event,
        void  *user_data);

struct abrt_inotify_watch *
abrt_inotify_watch_init(const char *path, int inotify_flags, abrt_inotify_watch_handler handler, void *user_data);

void
abrt_inotify_watch_destroy(struct abrt_inotify_watch *watch);

void
abrt_inotify_watch_reset(struct abrt_inotify_watch *watch, const char *path, int inotify_flags);

#endif /*_ABRT_INOTIFY_H_*/
