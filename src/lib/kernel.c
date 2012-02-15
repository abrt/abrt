/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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

char *koops_extract_version(const char *linepointer)
{
    if (strstr(linepointer, "Pid")
     || strstr(linepointer, "comm")
     || strstr(linepointer, "CPU")
     || strstr(linepointer, "REGS")
     || strstr(linepointer, "EFLAGS")
    ) {
        char* start;
        char* end;

        start = strstr(linepointer, "2.6.");
        if (!start)
            start = strstr(linepointer, "3.");
        if (start)
        {
            end = strchr(start, ')');
            if (!end)
                end = strchrnul(start, ' ');
            return xstrndup(start, end-start);
        }
    }

    return NULL;
}

/* reading /proc/sys/kernel/tainted file after an oops is ALWAYS going
 * to show it as tainted.
 *
 * https://bugzilla.redhat.com/show_bug.cgi?id=724838
 */

/**
 *	print_tainted - return a string to represent the kernel taint state.
 *
 *  'P' - Proprietary module has been loaded.
 *  'F' - Module has been forcibly loaded.
 *  'S' - SMP with CPUs not designed for SMP.
 *  'R' - User forced a module unload.
 *  'M' - System experienced a machine check exception.
 *  'B' - System has hit bad_page.
 *  'U' - Userspace-defined naughtiness.
 *  'D' - Kernel has oopsed before
 *  'A' - ACPI table overridden.
 *  'W' - Taint on warning.
 *  'C' - modules from drivers/staging are loaded.
 *  'I' - Working around severe firmware bug.
 *  'H' - Hardware is unsupported.
 *   T  - Tech_preview
 */

#if 0 /* unused */
static char *turn_off_flag(char *flags, char flag)
{
    size_t len = strlen(flags);
    for (int i = 0; i < len; ++i)
    {
        if (flags[i] == flag)
            flags[i] = ' ';
    }

    return flags;
}
#endif

char *kernel_tainted_short(const char *kernel_bt)
{
    /* example of flags: |G    B      | */
    char *tainted = strstr(kernel_bt, "Tainted: ");
    if (!tainted)
        return NULL;

    /* 12 == count of flags */
    char *tnt = xstrndup(tainted + strlen("Tainted: "), 12);

    return tnt;
}

#if 0 /* unused */
static const char *const tnts_long[] = {
    "Proprietary module has been loaded.",
    "Module has been forcibly loaded.",
    "SMP with CPUs not designed for SMP.",
    "User forced a module unload.",
    "System experienced a machine check exception.",
    "System has hit bad_page.",
    "Userspace-defined naughtiness.",
    "Kernel has oopsed before.",
    "ACPI table overridden.",
    "Taint on warning.",
    "Modules from drivers/staging are loaded.",
    "Working around severe firmware bug.",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Hardware is unsupported.",
    "Tech_preview",
};

GList *kernel_tainted_long(unsigned tainted)
{
    int i = 0;
    GList *tnt = NULL;

    while (tainted)
    {
        if ((0x1 & tainted) && tnts_long[i])
            tnt = g_list_append(tnt, xstrdup(tnts_long[i]));

        ++i;
        tainted >>= 1;
    }

    return tnt;
}
#endif

