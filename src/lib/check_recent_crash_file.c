/*
    Copyright (C) 2012	RedHat inc.

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

/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)

int check_recent_crash_file(const char *filename, const char *executable)
{
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
        return 0;

    int ex_len = strlen(executable);
    struct stat sb;
    int sz;

    fstat(fd, &sb); /* !paranoia. this can't fail. */
    if (sb.st_size != 0 /* if it wasn't created by us just now... */
     && (unsigned)(time(NULL) - sb.st_mtime) < 20 /* and is relatively new [is 20 sec ok?] */
    ) {
        char buf[ex_len + 2];
        sz = read(fd, buf, ex_len + 1);
        if (sz > 0)
        {
            buf[sz] = '\0';
            if (strcmp(executable, buf) == 0)
            {
                close(fd);
                return 1;
            }
        }
        lseek(fd, 0, SEEK_SET);
    }
    sz = write(fd, executable, ex_len);
    if (sz >= 0)
        IGNORE_RESULT(ftruncate(fd, sz));

    close(fd);
    return 0;
}
