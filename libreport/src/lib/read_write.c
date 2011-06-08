/*
 * Utility routines.
 *
 * Licensed under GPLv2 or later, see file COPYING in this tarball for details.
 */
#include "libreport.h"

/* Die with an error message if we can't read the entire buffer. */
void xread(int fd, void *buf, size_t count)
{
    if (count)
    {
        ssize_t size = full_read(fd, buf, count);
        if ((size_t)size != count)
            error_msg_and_die("short read");
    }
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
    ssize_t n;

    do {
        n = read(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}

ssize_t safe_write(int fd, const void *buf, size_t count)
{
    ssize_t n;

    do {
        n = write(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}

ssize_t full_read(int fd, void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len)
    {
        cc = safe_read(fd, buf, len);

        if (cc < 0)
        {
            if (total)
            {
                /* we already have some! */
                /* user can do another read to know the error code */
                return total;
            }
            return cc; /* read() returns -1 on failure. */
        }
        if (cc == 0)
            break;
        buf = ((char *)buf) + cc;
        total += cc;
        len -= cc;
    }

    return total;
}

ssize_t full_write(int fd, const void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len)
    {
        cc = safe_write(fd, buf, len);

        if (cc < 0)
        {
            if (total)
            {
                /* we already wrote some! */
                /* user can do another write to know the error code */
                return total;
            }
            return cc;  /* write() returns -1 on failure. */
        }

        total += cc;
        buf = ((const char *)buf) + cc;
        len -= cc;
    }

    return total;
}

ssize_t full_write_str(int fd, const char *buf)
{
    return full_write(fd, buf, strlen(buf));
}
