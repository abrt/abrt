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

int ndelay_on(int fd);
int ndelay_off(int fd);
int close_on_exec_on(int fd);

void* xcalloc(size_t nmemb, size_t size);
void* xmalloc(size_t size);
void* xrealloc(void *ptr, size_t size);
void* xzalloc(size_t size);
char* xstrdup(const char *s);
char* xstrndup(const char *s, int n);

void xpipe(int filedes[2]);
void xdup(int from);
void xdup2(int from, int to);
void xmove_fd(int from, int to);

void xwrite(int fd, const void *buf, size_t count);
void xwrite_str(int fd, const char *str);

off_t xlseek(int fd, off_t offset, int whence);

void xchdir(const char *path);

char* xvasprintf(const char *format, va_list p);
char* xasprintf(const char *format, ...);

void xsetenv(const char *key, const char *value);
int xsocket(int domain, int type, int protocol);
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
void xlisten(int s, int backlog);
ssize_t xsendto(int s, const void *buf, size_t len,
                const struct sockaddr *to, socklen_t tolen);

void xstat(const char *name, struct stat *stat_buf);

int xopen3(const char *pathname, int flags, int mode);
int xopen(const char *pathname, int flags);
void xunlink(const char *pathname);

/* Just testing dent->d_type == DT_REG is wrong: some filesystems
 * do not report the type, they report DT_UNKNOWN for every dirent
 * (and this is not a bug in filesystem, this is allowed by standards).
 * This function handles this case. Note: it returns 0 on symlinks
 * even if they point to regular files.
 */
int is_regular_file(struct dirent *dent, const char *dirname);
bool dot_or_dotdot(const char *filename);
char *last_char_is(const char *s, int c);

bool string_to_bool(const char *s);

void xseteuid(uid_t euid);
void xsetegid(gid_t egid);
void xsetreuid(uid_t ruid, uid_t euid);
void xsetregid(gid_t rgid, gid_t egid);

/* Returns getpwuid(uid)->pw_dir or NULL */
const char *get_home_dir(uid_t uid);

#ifdef __cplusplus
}
#endif

#endif
