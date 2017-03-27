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
#include <sys/un.h>
#include "libabrt.h"

void notify_new_path(const char *path)
{
    /* Ignore results and don't wait for response -> NULL */
    notify_new_path_with_response(path, NULL);
}

int notify_new_path_with_response(const char *path, char **message)
{
    int retval;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        retval = -errno;
        perror_msg("socket(AF_UNIX)");
        return retval;
    }

    struct sockaddr_un sunx;
    memset(&sunx, 0, sizeof(sunx));
    sunx.sun_family = AF_UNIX;
    strcpy(sunx.sun_path, VAR_RUN"/abrt/abrt.socket");

    if (connect(fd, (struct sockaddr *)&sunx, sizeof(sunx)))
    {
        retval = -errno;
        perror_msg("connect('%s')", sunx.sun_path);
        close(fd);
        return retval;
    }

    full_write_str(fd, "POST /creation_notification HTTP/1.1\r\n\r\n");
    full_write_str(fd, path);

    /*
     * This sends FIN packet. Without it, close() may result in RST instead.
     * Not really needed on AF_UNIX, just a bit of TCP-induced paranoia
     * aka "good practice".
     */
    shutdown(fd, SHUT_WR);
    if (message == NULL)
    {
        close(fd);
        return 0;
    }

    *message = xmalloc_read(fd, NULL);
    if (*message == NULL)
    {
        log_info("abrtd response could not be received");
        return -EBADMSG;
    }

    close(fd);

    unsigned code = 0;

    if (sscanf(*message, "HTTP/1.1 %u ", &code) != 1)
    {
        log_info("abrtd response does not contain HTTP code");
        return -EBADMSG;
    }

    /* Verify possible casting to int. */
    if (code > INT_MAX)
    {
        log_info("abrtd response HTTP code is out of range");
        return -EBADMSG;
    }

    char *data = strchr(*message, '\n');
    if (data == NULL)
    {
        log_info("abrtd response is missing the first new line");
        return -EBADMSG;
    }

    data = strchr(data + 1, '\n');
    if (data == NULL)
    {
        log_info("abrtd response is missing the second new line");
        return -EBADMSG;
    }

    memmove(*message, data + 1, strlen(data));

    /* If code is greater than INT_MAX, -EBADMSG is returned. */
    return (int)code;
}
