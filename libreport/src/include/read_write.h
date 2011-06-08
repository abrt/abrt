/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

#ifndef READ_WRITE_H
#define READ_WRITE_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif


// NB: will return short read on error, not -1,
// if some data was read before error occurred
#define xread abrt_xread
void xread(int fd, void *buf, size_t count);

#define safe_read abrt_safe_read
ssize_t safe_read(int fd, void *buf, size_t count);
#define safe_write abrt_safe_write
ssize_t safe_write(int fd, const void *buf, size_t count);

#define full_read abrt_full_read
ssize_t full_read(int fd, void *buf, size_t count);
#define full_write abrt_full_write
ssize_t full_write(int fd, const void *buf, size_t count);

#define full_write_str abrt_full_write_str
ssize_t full_write_str(int fd, const char *buf);

#ifdef __cplusplus
}
#endif

#endif
