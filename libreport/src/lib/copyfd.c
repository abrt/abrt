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

#define CONFIG_FEATURE_COPYBUF_KB 4

static const char msg_write_error[] = "write error";
static const char msg_read_error[] = "read error";

static off_t full_fd_action(int src_fd, int dst_fd, off_t size, int flags)
{
	int status = -1;
	off_t total = 0;
	int last_was_seek = 0;
#if CONFIG_FEATURE_COPYBUF_KB <= 4
	char buffer[CONFIG_FEATURE_COPYBUF_KB * 1024];
	enum { buffer_size = sizeof(buffer) };
#else
	char *buffer;
	int buffer_size;

	/* We want page-aligned buffer, just in case kernel is clever
	 * and can do page-aligned io more efficiently */
	buffer = mmap(NULL, CONFIG_FEATURE_COPYBUF_KB * 1024,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANON,
			/* ignored: */ -1, 0);
	buffer_size = CONFIG_FEATURE_COPYBUF_KB * 1024;
	if (buffer == MAP_FAILED) {
		buffer = alloca(4 * 1024);
		buffer_size = 4 * 1024;
	}
#endif

	if (src_fd < 0)
		goto out;

	if (!size) {
		size = buffer_size;
		status = 1; /* copy until eof */
	}

	while (1) {
		ssize_t rd;

		rd = safe_read(src_fd, buffer, size > buffer_size ? buffer_size : size);

		if (!rd) { /* eof - all done */
			if (last_was_seek) {
				if (lseek(dst_fd, -1, SEEK_CUR) < 0
				 || safe_write(dst_fd, "", 1) != 1
				) {
					perror_msg("%s", msg_write_error);
					break;
				}
			}
			status = 0;
			break;
		}
		if (rd < 0) {
			perror_msg("%s", msg_read_error);
			break;
		}
		/* dst_fd == -1 is a fake, else... */
		if (dst_fd >= 0) {
			if (flags & COPYFD_SPARSE) {
				ssize_t cnt = rd;
				while (--cnt >= 0)
					if (buffer[cnt] != 0)
						goto need2write;
				if (lseek(dst_fd, rd, SEEK_CUR) < 0) {
					flags &= ~COPYFD_SPARSE;
					goto need2write;
				}
				last_was_seek = 1;
			} else {
 need2write:
                                {
				    ssize_t wr = full_write(dst_fd, buffer, rd);
				    if (wr < rd) {
				        perror_msg("%s", msg_write_error);
				        break;
				    }
				    last_was_seek = 0;
                                }
			}
		}
		total += rd;
		if (status < 0) { /* if we aren't copying till EOF... */
			size -= rd;
			if (!size) {
				/* 'size' bytes copied - all done */
				status = 0;
				break;
			}
		}
	}
 out:

#if CONFIG_FEATURE_COPYBUF_KB > 4
	if (buffer_size != 4 * 1024)
		munmap(buffer, buffer_size);
#endif
	return status ? -1 : total;
}

off_t copyfd_size(int fd1, int fd2, off_t size, int flags)
{
	if (size) {
		return full_fd_action(fd1, fd2, size, flags);
	}
	return 0;
}

void copyfd_exact_size(int fd1, int fd2, off_t size)
{
	off_t sz = copyfd_size(fd1, fd2, size, /*flags:*/ 0);
	if (sz == size)
		return;
	if (sz != -1)
		error_msg_and_die("short read");
	/* if sz == -1, copyfd_XX already complained */
	xfunc_die();
}

off_t copyfd_eof(int fd1, int fd2, int flags)
{
	return full_fd_action(fd1, fd2, 0, flags);
}

off_t copy_file(const char *src_name, const char *dst_name, int mode)
{
    off_t r;
    int src = open(src_name, O_RDONLY);
    if (src < 0)
    {
        perror_msg("Can't open '%s'", src_name);
        return -1;
    }
    int dst = open(dst_name, O_WRONLY | O_TRUNC | O_CREAT, mode);
    if (dst < 0)
    {
        close(src);
        perror_msg("Can't open '%s'", dst_name);
        return -1;
    }
    r = copyfd_eof(src, dst, /*flags:*/ 0);
    close(src);
    close(dst);
    return r;
}
