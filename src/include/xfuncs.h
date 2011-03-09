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

#define ndelay_on abrt_ndelay_on
int ndelay_on(int fd);
#define ndelay_off abrt_ndelay_off
int ndelay_off(int fd);
#define close_on_exec_on abrt_close_on_exec_on
int close_on_exec_on(int fd);

#define xmalloc abrt_xmalloc
void* xmalloc(size_t size);
#define xrealloc abrt_xrealloc
void* xrealloc(void *ptr, size_t size);
#define xzalloc abrt_xzalloc
void* xzalloc(size_t size);
#define xstrdup abrt_xstrdup
char* xstrdup(const char *s);
#define xstrndup abrt_xstrndup
char* xstrndup(const char *s, int n);

#define xpipe abrt_xpipe
void xpipe(int filedes[2]);
#define xdup abrt_xdup
void xdup(int from);
#define xdup2 abrt_xdup2
void xdup2(int from, int to);
#define xmove_fd abrt_xmove_fd
void xmove_fd(int from, int to);

#define xwrite abrt_xwrite
void xwrite(int fd, const void *buf, size_t count);
#define xwrite_str abrt_xwrite_str
void xwrite_str(int fd, const char *str);

#define xlseek abrt_xlseek
off_t xlseek(int fd, off_t offset, int whence);

#define xchdir abrt_xchdir
void xchdir(const char *path);

#define xvasprintf abrt_xvasprintf
char* xvasprintf(const char *format, va_list p);
#define xasprintf abrt_xasprintf
char* xasprintf(const char *format, ...);

#define xsetenv abrt_xsetenv
void xsetenv(const char *key, const char *value);
/*
 * Utility function to unsetenv a string which was possibly putenv'ed.
 * The problem here is that "natural" optimization:
 * strchrnul(var_val, '=')[0] = '\0';
 * unsetenv(var_val);
 * is BUGGY: if string was put into environment via putenv,
 * its modification (s/=/NUL/) is illegal, and unsetenv will fail to unset it.
 * Of course, saving/restoring the char wouldn't work either.
 * This helper creates a copy up to '=', unsetenv's it, and frees:
 */
#define safe_unsetenv abrt_safe_unsetenv
void safe_unsetenv(const char *var_val);

#define xsocket abrt_xsocket
int xsocket(int domain, int type, int protocol);
#define xbind abrt_xbind
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
#define xlisten abrt_xlisten
void xlisten(int s, int backlog);
#define xsendto abrt_xsendto
ssize_t xsendto(int s, const void *buf, size_t len,
                const struct sockaddr *to, socklen_t tolen);

#define xstat abrt_xstat
void xstat(const char *name, struct stat *stat_buf);

#define xopen3 abrt_xopen3
int xopen3(const char *pathname, int flags, int mode);
#define xopen abrt_xopen
int xopen(const char *pathname, int flags);
#define xunlink abrt_xunlink
void xunlink(const char *pathname);

/* Just testing dent->d_type == DT_REG is wrong: some filesystems
 * do not report the type, they report DT_UNKNOWN for every dirent
 * (and this is not a bug in filesystem, this is allowed by standards).
 * This function handles this case. Note: it returns 0 on symlinks
 * even if they point to regular files.
 */
#define is_regular_file abrt_is_regular_file
int is_regular_file(struct dirent *dent, const char *dirname);

#define dot_or_dotdot abrt_dot_or_dotdot
bool dot_or_dotdot(const char *filename);
#define last_char_is abrt_last_char_is
char *last_char_is(const char *s, int c);

#define string_to_bool abrt_string_to_bool
bool string_to_bool(const char *s);

#define xseteuid abrt_xseteuid
void xseteuid(uid_t euid);
#define xsetegid abrt_xsetegid
void xsetegid(gid_t egid);
#define xsetreuid abrt_xsetreuid
void xsetreuid(uid_t ruid, uid_t euid);
#define xsetregid abrt_xsetregid
void xsetregid(gid_t rgid, gid_t egid);

/* Returns getpwuid(uid)->pw_dir or NULL */
#define get_home_dir abrt_get_home_dir
const char *get_home_dir(uid_t uid);

#ifdef __cplusplus
}
#endif

#endif
