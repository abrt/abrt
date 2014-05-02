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
#include <satyr/stacktrace.h>
#include <satyr/thread.h>

#include "libabrt.h"

/*
 * extract_oops tries to find oops signatures in a log
 */

struct line_info {
    char *ptr;
    char level;
};

static void record_oops(GList **oops_list, struct line_info* lines_info, int oopsstart, int oopsend)
{
    int q;
    int len;
    int rv = 1;

    len = 2;
    for (q = oopsstart; q <= oopsend; q++)
        len += strlen(lines_info[q].ptr) + 1;

    /* too short oopses are invalid */
    if (len > 100)
    {
        char *oops = (char*)xzalloc(len);
        char *dst = oops;
        char *version = NULL;
        for (q = oopsstart; q <= oopsend; q++)
        {
            if (!version)
                version = koops_extract_version(lines_info[q].ptr);
            if (lines_info[q].ptr[0])
            {
                dst = stpcpy(dst, lines_info[q].ptr);
                dst = stpcpy(dst, "\n");
            }
        }
        if ((dst - oops) > 100)
        {
            *oops_list = g_list_append(
                        *oops_list,
                        xasprintf("%s\n%s", (version ? version : "undefined"), oops)
            );
        }
        else
        {
            /* too short oopses are invalid */
            rv = 0;
        }
        free(oops);
        free(version);
    }

    VERB3 if (rv == 0) log("Dropped oops: too short");
}

void koops_extract_oopses(GList **oops_list, char *buffer, size_t buflen)
{
    char *c;
    int linecount = 0;
    int lines_info_size = 0;
    struct line_info *lines_info = NULL;

    /* Split buffer into lines */

    if (buflen != 0)
            buffer[buflen - 1] = '\n';  /* the buffer usually ends with \n, but let's make sure */
    c = buffer;
    while (c < buffer + buflen)
    {
        char linelevel;
        char *c9;
        char *colon;

        linecount++;
        c9 = (char*)memchr(c, '\n', buffer + buflen - c); /* a \n will always be found */
        assert(c9);
        *c9 = '\0'; /* turn the \n into a string termination */
        if (c9 == c)
            goto next_line;

        /* Is it a syslog file (/var/log/messages or similar)?
         * Even though _usually_ it looks like "Nov 19 12:34:38 localhost kernel: xxx",
         * some users run syslog in non-C locale:
         * "2010-02-22T09:24:08.156534-08:00 gnu-4 gnome-session[2048]: blah blah"
         *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ !!!
         * We detect it by checking for N:NN:NN pattern in first 15 chars
         * (and this still is not good enough... false positive: "pci 0000:15:00.0: PME# disabled")
         */
        colon = strchr(c, ':');
        if (colon && colon > c && colon < c + 15
         && isdigit(colon[-1]) /* N:... */
         && isdigit(colon[1]) /* ...N:NN:... */
         && isdigit(colon[2])
         && colon[3] == ':'
         && isdigit(colon[4]) /* ...N:NN:NN... */
         && isdigit(colon[5])
        ) {
            /* It's syslog file, not a bare dmesg */

            /* Skip non-kernel lines */
            char *kernel_str = strstr(c, "kernel: ");
            if (!kernel_str)
            {
                /* if we see our own marker:
                 * "hostname abrt: Kerneloops: Reported 1 kernel oopses to Abrt"
                 * we know we submitted everything upto here already */
                if (strstr(c, "kernel oopses to Abrt"))
                {
                    VERB3 log("Found our marker at line %d", linecount);
                    free(lines_info);
                    lines_info = NULL;
                    lines_info_size = 0;
                    list_free_with_free(*oops_list);
                    *oops_list = NULL;
                }
                goto next_line;
            }
            c = kernel_str + sizeof("kernel: ")-1;
        }

        linelevel = 0;
        /* store and remove kernel log level */
        if (*c == '<' && c[1] && c[2] == '>')
        {
            linelevel = c[1];
            c += 3;
        }
        /* remove jiffies time stamp counter if present
         * jiffies are unsigned long, so it can be 2^64 long, which is
         * 20 decimal digits*/
        if (*c == '[')
        {
            char *c2 = strchr(c, '.');
            char *c3 = strchr(c, ']');
            if (c2 && c3 && (c2 < c3) && (c3-c) < 21 && (c2-c) < 8)
            {
                c = c3 + 1;
                if (*c == ' ')
                        c++;
            }
        }
        if ((lines_info_size & 0xfff) == 0)
        {
            lines_info = xrealloc(lines_info, (lines_info_size + 0x1000) * sizeof(lines_info[0]));
        }
        lines_info[lines_info_size].ptr = c;
        lines_info[lines_info_size].level = linelevel;
        lines_info_size++;
next_line:
        c = c9 + 1;
    }

    /* Analyze lines */

    int i;
    char prevlevel = 0;
    int oopsstart = -1;
    int inbacktrace = 0;

    i = 0;
    while (i < lines_info_size)
    {
        char *curline = lines_info[i].ptr;

        if (curline == NULL)
        {
            i++;
            continue;
        }
        while (*curline == ' ')
            curline++;

        if (oopsstart < 0)
        {
            /* Find start-of-oops markers */
            /* In some comparisons, we skip 1st letter, to avoid dealing with
             * changes in capitalization in kernel. For example, I see that
             * current kernel git (at 2011-01-01) has both "kernel BUG at ..."
             * and "Kernel BUG at ..." messages, and I don't want to change
             * the code below whenever kernel is changed to use "K" (or "k")
             * uniformly.
             */
            if (strstr(curline, /*g*/ "eneral protection fault:"))
                oopsstart = i;
            else if (strstr(curline, "BUG:"))
                oopsstart = i;
            else if (strstr(curline, /*k*/ "ernel BUG at"))
                oopsstart = i;
            /* WARN_ON() generated message */
            else if (strstr(curline, "WARNING: at "))
                oopsstart = i;
            else if (strstr(curline, /*u*/ "nable to handle kernel"))
                oopsstart = i;
            else if (strstr(curline, /*d*/ "ouble fault:"))
                oopsstart = i;
            else if (strstr(curline, "do_IRQ: stack overflow:"))
                oopsstart = i;
            else if (strstr(curline, "RTNL: assertion failed"))
                 oopsstart = i;
            else if (strstr(curline, /*e*/ "eek! page_mapcount(page) went negative!"))
                oopsstart = i;
            else if (strstr(curline, /*n*/ "ear stack overflow (cur:"))
                oopsstart = i;
            else if (strstr(curline, /*b*/ "adness at"))
                oopsstart = i;
            else if (strstr(curline, "NETDEV WATCHDOG"))
                oopsstart = i;
            else if (strstr(curline, /*s*/ "ysctl table check failed"))
                oopsstart = i;
            else if (strstr(curline, "INFO: possible recursive locking detected"))
                oopsstart = i;
            // Not needed: "--[ cut here ]--" is always followed
            // by "Badness at", "kernel BUG at", or "WARNING: at" string
            //else if (strstr(curline, "------------[ cut here ]------------"))
            //  oopsstart = i;
            else if (strstr(curline, "list_del corruption"))
                oopsstart = i;
            else if (strstr(curline, "list_add corruption"))
                oopsstart = i;
            /* "irq NN: nobody cared..." */
            else if (strstr(curline, ": nobody cared"))
                oopsstart = i;
            else if (strstr(curline, "IRQ handler type mismatch"))
                oopsstart = i;

            if (oopsstart >= 0)
            {
                /* debug information */
                VERB3 {
                    log("Found oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
                    if (oopsstart != i)
                            log("Trigger line is %d: '%s'", i, c);
                }
                /* try to find the end marker */
                int i2 = i + 1;
                while (i2 < lines_info_size && i2 < (i+50))
                {
                    if (strstr(lines_info[i2].ptr, "---[ end trace"))
                    {
                        inbacktrace = 1;
                        i = i2;
                        break;
                    }
                    i2++;
                }
            }
        }

        /* Are we entering a call trace part? */
        /* a call trace starts with "Call Trace:" or with the " [<.......>] function+0xFF/0xAA" pattern */
        if (oopsstart >= 0 && !inbacktrace)
        {
            if (strstr(curline, "Call Trace:"))
                inbacktrace = 1;
            else
            if (strnlen(curline, 9) > 8
             && curline[0] == '[' && curline[1] == '<'
             && strstr(curline, ">]")
             && strstr(curline, "+0x")
             && strstr(curline, "/0x")
            ) {
                inbacktrace = 1;
            }
        }

        /* Are we at the end of an oops? */
        else if (oopsstart >= 0 && inbacktrace)
        {
            int oopsend = INT_MAX;

            /* line needs to start with " [" or have "] [" if it is still a call trace */
            /* example: "[<ffffffffa006c156>] radeon_get_ring_head+0x16/0x41 [radeon]" */
            if (curline[0] != '['
             && !strstr(curline, "] [")
             && !strstr(curline, "--- Exception")
             && !strstr(curline, "LR =")
             && !strstr(curline, "<#DF>")
             && !strstr(curline, "<IRQ>")
             && !strstr(curline, "<EOI>")
             && !strstr(curline, "<<EOE>>")
             && strncmp(curline, "Code: ", 6) != 0
             && strncmp(curline, "RIP ", 4) != 0
             && strncmp(curline, "RSP ", 4) != 0
            ) {
                oopsend = i-1; /* not a call trace line */
            }
            /* oops lines are always more than 8 chars long */
            else if (strnlen(curline, 8) < 8)
                oopsend = i-1;
            /* single oopses are of the same loglevel */
            else if (lines_info[i].level != prevlevel)
                oopsend = i-1;
            else if (strstr(curline, "Instruction dump:"))
                oopsend = i;
            /* if a new oops starts, this one has ended */
            else if (strstr(curline, "WARNING: at ") && oopsstart != i) /* WARN_ON() generated message */
                oopsend = i-1;
            else if (strstr(curline, "Unable to handle") && oopsstart != i)
                oopsend = i-1;
            /* kernel end-of-oops marker (not including marker itself) */
            else if (strstr(curline, "---[ end trace"))
                oopsend = i-1;

            if (oopsend <= i)
            {
                VERB3 log("End of oops at line %d (%d): '%s'", oopsend, i, lines_info[oopsend].ptr);
                record_oops(oops_list, lines_info, oopsstart, oopsend);
                oopsstart = -1;
                inbacktrace = 0;
            }
        }

        prevlevel = lines_info[i].level;
        i++;

        if (oopsstart >= 0)
        {
            /* Do we have a suspiciously long oops? Cancel it.
             * Bumped from 60 to 80 (see examples/oops_recursive_locking1.test)
             */
            if (i - oopsstart > 80)
            {
                inbacktrace = 0;
                oopsstart = -1;
                VERB3 log("Dropped oops, too long");
                continue;
            }
            if (!inbacktrace && i - oopsstart > 40)
            {
                /*inbacktrace = 0; - already is */
                oopsstart = -1;
                VERB3 log("Dropped oops, too long");
                continue;
            }
        }
    } /* while (i < lines_info_size) */

    /* process last oops if we have one */
    if (oopsstart >= 0 && inbacktrace)
    {
        int oopsend = i-1;
        VERB3 log("End of oops at line %d (end of file): '%s'", oopsend, lines_info[oopsend].ptr);
        record_oops(oops_list, lines_info, oopsstart, oopsend);
    }

    free(lines_info);
}

int koops_hash_str_ext(char result[SHA1_RESULT_LEN*2 + 1], const char *oops_buf, int frame_count, int duphash_flags)
{
    char *hash_str = NULL, *error = NULL;
    int bad = 0;

    struct sr_stacktrace *stacktrace = sr_stacktrace_parse(SR_REPORT_KERNELOOPS,
                                                           oops_buf, &error);
    if (!stacktrace)
    {
        VERB3 log("Failed to parse koops: %s", error);
        free(error);
        bad = 1;
        goto end;
    }

    struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
    {
        VERB3 log("Failed to find crash thread");
        bad = 1;
        goto end;
    }

    if (g_verbose >= 3)
    {
        hash_str = sr_thread_get_duphash(thread, frame_count, NULL,
                                         duphash_flags|SR_DUPHASH_NOHASH);
        if (hash_str)
            log("Generating duphash: %s", hash_str);
        free(hash_str);
    }

    hash_str = sr_thread_get_duphash(thread, frame_count, NULL, duphash_flags);
    if (hash_str)
    {
        strncpy(result, hash_str, SHA1_RESULT_LEN*2);
        result[SHA1_RESULT_LEN*2] = '\0';
        free(hash_str);
    }
    else
        bad = 1;

end:
    sr_stacktrace_free(stacktrace);
    return bad;
}

int koops_hash_str(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *oops_buf)
{
    const int frame_count = 5;
    const int duphash_flags = SR_DUPHASH_NONORMALIZE|SR_DUPHASH_KOOPS_COMPAT;
    return koops_hash_str_ext(hash_str, oops_buf, frame_count, duphash_flags);
}

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

