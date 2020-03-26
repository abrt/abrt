/*
    Copyright (C) 2009  RedHat inc.

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
#include "libabrt.h"

int abrt_daemon_is_ok()
{
    int fd = open(VAR_RUN"/abrt/abrtd.pid", O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }

    char pid[sizeof(pid_t)*3 + 2];
    int len = read(fd, pid, sizeof(pid)-1);
    close(fd);
    if (len <= 0)
        return 0;

    pid[len] = '\0';
    *strchrnul(pid, '\n') = '\0';
    /* paranoia: only allow non-empty numeric strings */
    unsigned i = 0;
    do
    {
        if (pid[i] < '0' || pid[i] > '9')
            return 0;
        i++;
    } while (pid[i] != '\0');

    char path[sizeof("/proc/%s/stat") + sizeof(pid)];
    sprintf(path, "/proc/%s/stat", pid);
    struct stat sb;
    if (stat(path, &sb) == -1)
    {
        return 0;
    }

    /* TODO: maybe readlink /proc/PID/exe and check that it is "xxx/abrt"? */

    return 1;
}
