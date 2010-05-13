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
#include "abrtlib.h"

#define VAR_RUN_PID_FILE        VAR_RUN"/abrtd.pid"

static char *append_escaped(char *start, const char *s)
{
    char hex_char_buf[] = "\\x00";

    *start++ = ' ';
    char *dst = start;
    const unsigned char *p = (unsigned char *)s;

    while (1)
    {
        const unsigned char *old_p = p;
        while (*p > ' ' && *p <= 0x7e && *p != '\"' && *p != '\'' && *p != '\\')
            p++;
        if (dst == start)
        {
            if (p != (unsigned char *)s && *p == '\0')
            {
                /* entire word does not need escaping and quoting */
                strcpy(dst, s);
                dst += strlen(s);
                return dst;
            }
            *dst++ = '\'';
        }

        strncpy(dst, (char *)old_p, (p - old_p));
        dst += (p - old_p);

        if (*p == '\0')
        {
            *dst++ = '\'';
            *dst = '\0';
            return dst;
        }
        const char *a;
        switch (*p)
        {
        case '\r': a = "\\r"; break;
        case '\n': a = "\\n"; break;
        case '\t': a = "\\t"; break;
        case '\'': a = "\\\'"; break;
        case '\"': a = "\\\""; break;
        case '\\': a = "\\\\"; break;
        case ' ': a = " "; break;
        default:
            hex_char_buf[2] = "0123456789abcdef"[*p >> 4];
            hex_char_buf[3] = "0123456789abcdef"[*p & 0xf];
            a = hex_char_buf;
        }
        strcpy(dst, a);
        dst += strlen(a);
        p++;
    }
}

// taken from kernel
#define COMMAND_LINE_SIZE 2048
char* get_cmdline(pid_t pid)
{
    char path[sizeof("/proc/%lu/cmdline") + sizeof(long)*3];
    char cmdline[COMMAND_LINE_SIZE];
    char escaped_cmdline[COMMAND_LINE_SIZE*4 + 4];

    escaped_cmdline[1] = '\0';
    sprintf(path, "/proc/%lu/cmdline", (long)pid);
    int fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        int len = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (len > 0)
        {
            cmdline[len] = '\0';
            char *src = cmdline;
            char *dst = escaped_cmdline;
            while ((src - cmdline) < len)
            {
                dst = append_escaped(dst, src);
                src += strlen(src) + 1;
            }
        }
    }

    return xstrdup(escaped_cmdline + 1); /* +1 skips extraneous leading space */
}

int daemon_is_ok()
{
    int fd = open(VAR_RUN_PID_FILE, O_RDONLY);
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
    /* paranoia: we don't want to check /proc//stat or /proc///stat */
    if (pid[0] == '\0' || pid[0] == '/')
        return 0;

    /* TODO: maybe readlink and check that it is "xxx/abrt"? */
    char path[sizeof("/proc/%s/stat") + sizeof(pid)];
    sprintf(path, "/proc/%s/stat", pid);
    struct stat sb;
    if (stat(path, &sb) == -1)
    {
        return 0;
    }

    return 1;
}
