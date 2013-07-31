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
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror_msg("socket(AF_UNIX)");
        return;
    }

    struct sockaddr_un sunx;
    memset(&sunx, 0, sizeof(sunx));
    sunx.sun_family = AF_UNIX;
    strcpy(sunx.sun_path, VAR_RUN"/abrt/abrt.socket");

    if (connect(fd, (struct sockaddr *)&sunx, sizeof(sunx)))
    {
        perror_msg("connect('%s')", sunx.sun_path);
        close(fd);
        return;
    }

    full_write_str(fd, "POST /creation_notification HTTP/1.1\r\n\r\n");
    full_write_str(fd, path);
    /*
     * This sends FIN packet. Without it, close() may result in RST instead.
     * Not really needed on AF_UNIX, just a bit of TCP-induced paranoia
     * aka "good practice".
     */
    shutdown(fd, SHUT_WR);
    close(fd);
}
