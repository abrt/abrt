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

/*
 * Utility routines.
 *
 */
#include "libreport.h"

/* Turn on nonblocking I/O on a fd */
int ndelay_on(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags & O_NONBLOCK)
        return 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int ndelay_off(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (!(flags & O_NONBLOCK))
        return 0;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int close_on_exec_on(int fd)
{
    return fcntl(fd, F_SETFD, FD_CLOEXEC);
}

// Die if we can't allocate size bytes of memory.
void* xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0)
        die_out_of_memory();
    return ptr;
}

// Die if we can't resize previously allocated memory.  (This returns a pointer
// to the new memory, which may or may not be the same as the old memory.
// It'll copy the contents to a new chunk and free the old one if necessary.)
void* xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL && size != 0)
        die_out_of_memory();
    return ptr;
}

// Die if we can't allocate and zero size bytes of memory.
void* xzalloc(size_t size)
{
    void *ptr = xmalloc(size);
    memset(ptr, 0, size);
    return ptr;
}

// Die if we can't copy a string to freshly allocated memory.
char* xstrdup(const char *s)
{
    char *t;
    if (s == NULL)
        return NULL;

    t = strdup(s);

    if (t == NULL)
        die_out_of_memory();

    return t;
}

// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
char* xstrndup(const char *s, int n)
{
    int m;
    char *t;

    /* We can just xmalloc(n+1) and strncpy into it, */
    /* but think about xstrndup("abc", 10000) wastage! */
    m = n;
    t = (char*) s;
    while (m)
    {
        if (!*t) break;
        m--;
        t++;
    }
    n -= m;
    t = (char*) xmalloc(n + 1);
    t[n] = '\0';

    return (char*) memcpy(t, s, n);
}

void xpipe(int filedes[2])
{
    if (pipe(filedes))
        perror_msg_and_die("can't create pipe");
}

void xdup(int from)
{
    if (dup(from) < 0)
        perror_msg_and_die("can't duplicate file descriptor");
}

void xdup2(int from, int to)
{
    if (dup2(from, to) != to)
        perror_msg_and_die("can't duplicate file descriptor");
}

// "Renumber" opened fd
void xmove_fd(int from, int to)
{
    if (from == to)
        return;
    xdup2(from, to);
    close(from);
}

// Die with an error message if we can't write the entire buffer.
void xwrite(int fd, const void *buf, size_t count)
{
    if (count == 0)
        return;
    ssize_t size = full_write(fd, buf, count);
    if ((size_t)size != count)
        error_msg_and_die("short write");
}

void xwrite_str(int fd, const char *str)
{
    xwrite(fd, str, strlen(str));
}

// Die with an error message if we can't lseek to the right spot.
off_t xlseek(int fd, off_t offset, int whence)
{
    off_t off = lseek(fd, offset, whence);
    if (off == (off_t)-1) {
        if (whence == SEEK_SET)
            perror_msg_and_die("lseek(%llu)", (long long)offset);
        perror_msg_and_die("lseek");
    }
    return off;
}

void xchdir(const char *path)
{
    if (chdir(path))
        perror_msg_and_die("chdir(%s)", path);
}

char* xvasprintf(const char *format, va_list p)
{
    int r;
    char *string_ptr;

#if 1
    // GNU extension
    r = vasprintf(&string_ptr, format, p);
#else
    // Bloat for systems that haven't got the GNU extension.
    va_list p2;
    va_copy(p2, p);
    r = vsnprintf(NULL, 0, format, p);
    string_ptr = xmalloc(r+1);
    r = vsnprintf(string_ptr, r+1, format, p2);
    va_end(p2);
#endif

    if (r < 0)
        die_out_of_memory();
    return string_ptr;
}

// Die with an error message if we can't malloc() enough space and do an
// sprintf() into that space.
char* xasprintf(const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = xvasprintf(format, p);
    va_end(p);

    return string_ptr;
}

void xsetenv(const char *key, const char *value)
{
    if (setenv(key, value, 1))
        die_out_of_memory();
}

void safe_unsetenv(const char *var_val)
{
    //char *name = xstrndup(var_val, strchrnul(var_val, '=') - var_val);
    //unsetenv(name);
    //free(name);

    /* Avoid malloc/free (name is usually very short) */
    unsigned len = strchrnul(var_val, '=') - var_val;
    char name[len + 1];
    memcpy(name, var_val, len);
    name[len] = '\0';
    unsetenv(name);
}

// Die with an error message if we can't open a new socket.
int xsocket(int domain, int type, int protocol)
{
    int r = socket(domain, type, protocol);
    if (r < 0)
    {
        const char *s = "INET";
        if (domain == AF_PACKET) s = "PACKET";
        if (domain == AF_NETLINK) s = "NETLINK";
        if (domain == AF_INET6) s = "INET6";
        perror_msg_and_die("socket(AF_%s)", s);
    }

    return r;
}

// Die with an error message if we can't bind a socket to an address.
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
    if (bind(sockfd, my_addr, addrlen))
        perror_msg_and_die("bind");
}

// Die with an error message if we can't listen for connections on a socket.
void xlisten(int s, int backlog)
{
    if (listen(s, backlog))
        perror_msg_and_die("listen");
}

// Die with an error message if sendto failed.
// Return bytes sent otherwise
ssize_t xsendto(int s, const void *buf, size_t len,
                const struct sockaddr *to,
                socklen_t tolen)
{
    ssize_t ret = sendto(s, buf, len, 0, to, tolen);
    if (ret < 0)
    {
        close(s);
        perror_msg_and_die("sendto");
    }
    return ret;
}

// xstat() - a stat() which dies on failure with meaningful error message
void xstat(const char *name, struct stat *stat_buf)
{
    if (stat(name, stat_buf))
        perror_msg_and_die("can't stat '%s'", name);
}

// Die if we can't open a file and return a fd
int xopen3(const char *pathname, int flags, int mode)
{
    int ret;
    ret = open(pathname, flags, mode);
    if (ret < 0)
        perror_msg_and_die("can't open '%s'", pathname);
    return ret;
}

// Die if we can't open an existing file and return a fd
int xopen(const char *pathname, int flags)
{
    return xopen3(pathname, flags, 0666);
}

void xunlink(const char *pathname)
{
    if (unlink(pathname))
        perror_msg_and_die("Can't remove file '%s'", pathname);
}

#if 0 //UNUSED
// Warn if we can't open a file and return a fd.
int open3_or_warn(const char *pathname, int flags, int mode)
{
    int ret;
    ret = open(pathname, flags, mode);
    if (ret < 0)
        perror_msg("can't open '%s'", pathname);
    return ret;
}

// Warn if we can't open a file and return a fd.
int open_or_warn(const char *pathname, int flags)
{
    return open3_or_warn(pathname, flags, 0666);
}
#endif

/* Just testing dent->d_type == DT_REG is wrong: some filesystems
 * do not report the type, they report DT_UNKNOWN for every dirent
 * (and this is not a bug in filesystem, this is allowed by standards).
 */
int is_regular_file(struct dirent *dent, const char *dirname)
{
    if (dent->d_type == DT_REG)
        return 1;
    if (dent->d_type != DT_UNKNOWN)
        return 0;

    char *fullname = xasprintf("%s/%s", dirname, dent->d_name);
    struct stat statbuf;
    int r = lstat(fullname, &statbuf);
    free(fullname);

    return r == 0 && S_ISREG(statbuf.st_mode);
}

/* Is it "." or ".."? */
/* abrtlib candidate */
bool dot_or_dotdot(const char *filename)
{
    if (filename[0] != '.') return false;
    if (filename[1] == '\0') return true;
    if (filename[1] != '.') return false;
    if (filename[2] == '\0') return true;
    return false;
}

/* Find out if the last character of a string matches the one given.
 * Don't underrun the buffer if the string length is 0.
 */
char *last_char_is(const char *s, int c)
{
    if (s && *s)
    {
        s += strlen(s) - 1;
        if ((unsigned char)*s == c)
            return (char*)s;
    }
    return NULL;
}

bool string_to_bool(const char *s)
{
    if (s[0] == '1' && s[1] == '\0')
        return true;
    if (strcasecmp(s, "on") == 0)
        return true;
    if (strcasecmp(s, "yes") == 0)
        return true;
    if (strcasecmp(s, "true") == 0)
        return true;
    return false;
}

void xseteuid(uid_t euid)
{
    if (seteuid(euid) != 0)
        perror_msg_and_die("can't set %cid %lu", 'u', (long)euid);
}

void xsetegid(gid_t egid)
{
    if (setegid(egid) != 0)
        perror_msg_and_die("can't set %cid %lu", 'g', (long)egid);
}

void xsetreuid(uid_t ruid, uid_t euid)
{
    if (setreuid(ruid, euid) != 0)
        perror_msg_and_die("can't set %cid %lu", 'u', (long)ruid);
}

void xsetregid(gid_t rgid, gid_t egid)
{
    if (setregid(rgid, egid) != 0)
        perror_msg_and_die("can't set %cid %lu", 'g', (long)rgid);
}

const char *get_home_dir(uid_t uid)
{
    struct passwd* pw = getpwuid(uid);
    // TODO: handle errno
    return pw ? pw->pw_dir : NULL;
}
