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
#ifndef ABRT_XFUNCS_H
#define ABRT_XFUNCS_H

#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int ndelay_on(int fd);
extern int ndelay_off(int fd);
extern int close_on_exec_on(int fd);

extern void* xmalloc(size_t size);
extern void* xrealloc(void *ptr, size_t size);
extern void* xzalloc(size_t size);
extern char* xstrdup(const char *s);
extern char* xstrndup(const char *s, int n);

extern void xpipe(int filedes[2]);
extern void xdup(int from);
extern void xdup2(int from, int to);
extern void xmove_fd(int from, int to);

extern void xwrite(int fd, const void *buf, size_t count);
extern void xwrite_str(int fd, const char *str);

extern off_t xlseek(int fd, off_t offset, int whence);

extern void xchdir(const char *path);

extern char* xvasprintf(const char *format, va_list p);
extern char* xasprintf(const char *format, ...);

extern void xsetenv(const char *key, const char *value);
extern int xsocket(int domain, int type, int protocol);
extern void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
extern void xlisten(int s, int backlog);
extern ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to, socklen_t tolen);
extern void xstat(const char *name, struct stat *stat_buf);

extern int xopen3(const char *pathname, int flags, int mode);
extern int xopen(const char *pathname, int flags);
extern void xunlink(const char *pathname);

/* Just testing dent->d_type == DT_REG is wrong: some filesystems
 * do not report the type, they report DT_UNKNOWN for every dirent
 * (and this is not a bug in filesystem, this is allowed by standards).
 * This function handles this case. Note: it returns 0 on symlinks
 * even if they point to regular files.
 */
extern int is_regular_file(struct dirent *dent, const char *dirname);
extern bool dot_or_dotdot(const char *filename);
extern char *last_char_is(const char *s, int c);

extern bool string_to_bool(const char *s);

extern void xseteuid(uid_t euid);
extern void xsetegid(gid_t egid);
extern void xsetreuid(uid_t ruid, uid_t euid);
extern void xsetregid(gid_t rgid, gid_t egid);

/* Returns getpwuid(uid)->pw_dir or NULL */
extern const char *get_home_dir(uid_t uid);

#ifdef __cplusplus
}
#endif

#endif
