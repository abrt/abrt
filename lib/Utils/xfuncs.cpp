/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#include "abrtlib.h"

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
	while (m) {
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
	if (count) {
		ssize_t size = full_write(fd, buf, count);
		if ((size_t)size != count)
			error_msg_and_die("short write");
	}
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

// Die with an error message if we can't malloc() enough space and do an
// sprintf() into that space.
char* xasprintf(const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

#if 1
	// GNU extension
	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);
#else
	// Bloat for systems that haven't got the GNU extension.
	va_start(p, format);
	r = vsnprintf(NULL, 0, format, p);
	va_end(p);
	string_ptr = xmalloc(r+1);
	va_start(p, format);
	r = vsnprintf(string_ptr, r+1, format, p);
	va_end(p);
#endif

	if (r < 0)
		die_out_of_memory();
	return string_ptr;
}

std::string ssprintf(const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

#if 1
	// GNU extension
	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);
#else
	// Bloat for systems that haven't got the GNU extension.
	va_start(p, format);
	r = vsnprintf(NULL, 0, format, p);
	va_end(p);
	string_ptr = xmalloc(r+1);
	va_start(p, format);
	r = vsnprintf(string_ptr, r+1, format, p);
	va_end(p);
#endif

	if (r < 0)
		die_out_of_memory();
	std::string res = string_ptr;
	free(string_ptr);
	return res;
}

void xsetenv(const char *key, const char *value)
{
	if (setenv(key, value, 1))
		die_out_of_memory();
}

// Die with an error message if we can't open a new socket.
int xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);

	if (r < 0) {
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
	if (bind(sockfd, my_addr, addrlen)) perror_msg_and_die("bind");
}

// Die with an error message if we can't listen for connections on a socket.
void xlisten(int s, int backlog)
{
	if (listen(s, backlog)) perror_msg_and_die("listen");
}

// Die with an error message if sendto failed.
// Return bytes sent otherwise
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen)
{
	ssize_t ret = sendto(s, buf, len, 0, to, tolen);
	if (ret < 0) {
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

std::string get_home_dir(int uid)
{
    struct passwd* pw = getpwuid(uid);
    return pw ? pw->pw_dir : "";
}

// Die if we can't open a file and return a fd
int xopen3(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		perror_msg_and_die("can't open '%s'", pathname);
	}
	return ret;
}

// Die if we can't open an existing file and return a fd
int xopen(const char *pathname, int flags)
{
	return xopen3(pathname, flags, 0666);
}

#if 0 //UNUSED
// Warn if we can't open a file and return a fd.
int open3_or_warn(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		perror_msg("can't open '%s'", pathname);
	}
	return ret;
}

// Warn if we can't open a file and return a fd.
int open_or_warn(const char *pathname, int flags)
{
	return open3_or_warn(pathname, flags, 0666);
}
#endif

void xunlink(const char *pathname)
{
	if (unlink(pathname))
		perror_msg_and_die("can't remove file '%s'", pathname);
}
