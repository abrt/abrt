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

#define _GNU_SOURCE 1 /* for strcasestr */
#include "libabrt.h"

/*
 * extract_oops tries to find oops signatures in a log
 */

struct line_info {
    char *ptr;
    char level;
};

/* Used to be 100, but some MCE oopses are short:
 * "CPU 0: Machine Check Exception: 0000000000000007"
 */
#define SANE_MIN_OOPS_LEN 30

static void record_oops(GList **oops_list, struct line_info* lines_info, int oopsstart, int oopsend)
{
    int q;
    int len;
    int rv = 1;

    len = 2;
    for (q = oopsstart; q <= oopsend; q++)
        len += strlen(lines_info[q].ptr) + 1;

    /* too short oopses are invalid */
    if (len > SANE_MIN_OOPS_LEN)
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
        if ((dst - oops) > SANE_MIN_OOPS_LEN)
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

/* In some comparisons, we skip 1st letter, to avoid dealing with
 * changes in capitalization in kernel. For example, I see that
 * current kernel git (at 2011-01-01) has both "kernel BUG at ..."
 * and "Kernel BUG at ..." messages, and I don't want to change
 * the code below whenever kernel is changed to use "K" (or "k")
 * uniformly.
 */
static const char *const s_koops_suspicious_strings[] = {
    "BUG:",
    "WARNING: at",
    "WARNING: CPU:",
    "INFO: possible recursive locking detected",
    /*k*/"ernel BUG at",
    "list_del corruption",
    "list_add corruption",
    "do_IRQ: stack overflow:",
    /*n*/"ear stack overflow (cur:",
    /*g*/"eneral protection fault",
    /*u*/"nable to handle kernel",
    /*d*/"ouble fault:",
    "RTNL: assertion failed",
    /*e*/"eek! page_mapcount(page) went negative!",
    /*b*/"adness at",
    "NETDEV WATCHDOG",
    /*s*/"ysctl table check failed",
    ": nobody cared",
    "IRQ handler type mismatch",
    /*
     * MCE examples for various CPUs/architectures (collected 2013-04):
     * arch/arc/kernel/traps.c:			die("Machine Check Exception", regs, address, cause);
     * arch/x86/kernel/cpu/mcheck/winchip.c:	printk(KERN_EMERG "CPU0: Machine Check Exception.\n");
     * arch/x86/kernel/cpu/mcheck/p5.c:		"CPU#%d: Machine Check Exception:  0x%8X (type 0x%8X).\n",
     * arch/x86/kernel/cpu/mcheck/mce.c:	pr_emerg(HW_ERR "CPU %d: Machine Check Exception: %Lx Bank %d: %016Lx\n",
     * drivers/edac/sb_edac.c:			printk("CPU %d: Machine Check Exception: %Lx Bank %d: %016Lx\n",
     *
     * MCEs can be fatal (they panic kernel) or not.
     * Fatal MCE are delivered as exception#18 to the CPU.
     * Non-fatal ones sometimes are delivered as exception#18;
     * other times they are silently recorded in magic MSRs, CPU is not alerted.
     * Linux kernel periodically (up to 5 mins interval) reads those MSRs
     * and if MCE is seen there, it is piped in binary form through
     * /dev/mcelog to whoever listens on it. (Such as mcelog tool in --daemon
     * mode; but cat </dev/mcelog would do too).
     *
     * "Machine Check Exception:" message is printed *only*
     * by fatal MCEs (so far, future kernels may be different).
     * It will be caught as vmcore if kdump is configured.
     *
     * Non-fatal MCEs have "[Hardware Error]: Machine check events logged"
     * message in kernel log.
     * When /dev/mcelog is read, *no additional kernel log messages appear*:
     * if we want more readable data, we must rely on other tools
     * (such as mcelog daemon consuming binary /dev/mcelog and writing
     * human-readable /var/log/mcelog).
     */
    "Machine Check Exception:",
    "Machine check events logged",

    /* X86 TRAPs */
    "divide error:",
    "bounds:",
    "coprocessor segment overrun:",
    "invalid TSS:",
    "segment not present:",
    "invalid opcode:",
    "alignment check:",
    "stack segment:",
    "fpu exception:",
    "simd exception:",
    "iret exception:",

    /* Termination */
    NULL
};

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
            if (c2 && c3 && (c2 < c3) && (c3-c) < 21)
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
            for (const char *const *str = s_koops_suspicious_strings; *str; ++str)
            {
                if (strstr(curline, *str))
                {
                    oopsstart = i;
                    break;
                }
            }

            if (oopsstart >= 0)
            {
                /* debug information */
                VERB3 log("Found oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
                if (oopsstart != i)
                        VERB3 log("Trigger line is %d: '%s'", i, c);
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
            if (strcasestr(curline, "Call Trace:")) /* yes, it must be case-insensitive */
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
             && !strstr(curline, "<NMI>")
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
            /* kernel end-of-oops marker (not including marker itself) */
            else if (strstr(curline, "---[ end trace"))
                oopsend = i-1;
            else
            {
                /* if a new oops starts, this one has ended */
                for (const char *const *str = s_koops_suspicious_strings; *str; ++str)
                {
                    if (strstr(curline, *str))
                    {
                        oopsend = i-1;
                        break;
                    }
                }
            }

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
                /* Used to drop oopses w/o backtraces, but some of them
                 * (MCEs, for example) don't have backtrace yet we still want to file them.
                 */
                VERB3 log("One-line oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
                record_oops(oops_list, lines_info, oopsstart, oopsstart);
                /*inbacktrace = 0; - already is */
                oopsstart = -1;
                continue;
            }
        }
    } /* while (i < lines_info_size) */

    /* process last oops if we have one */
    if (oopsstart >= 0)
    {
        if (inbacktrace)
        {
            int oopsend = i-1;
            VERB3 log("End of oops at line %d (end of file): '%s'", oopsend, lines_info[oopsend].ptr);
            record_oops(oops_list, lines_info, oopsstart, oopsend);
        }
        else
        {
            VERB3 log("One-line oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
            record_oops(oops_list, lines_info, oopsstart, oopsstart);
        }
    }

    free(lines_info);
}

int koops_hash_str_ext(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *oops_buf, int frame_count, int only_reliable)
{
    struct strbuf *kernel_bt = strbuf_new();

    // Example of call trace part of oops:
    // Call Trace:
    // [<f88e11c7>] ? radeon_cp_resume+0x7d/0xbc [radeon]
    // [<f88745f8>] ? drm_ioctl+0x1b0/0x225 [drm]
    // [<f88e114a>] ? radeon_cp_resume+0x0/0xbc [radeon]
    // [<c049b1c0>] ? vfs_ioctl+0x50/0x69
    // [<c049b414>] ? do_vfs_ioctl+0x23b/0x247
    // [<c0460a56>] ? audit_syscall_entry+0xf9/0x123
    // [<c049b460>] ? sys_ioctl+0x40/0x5c
    // [<c0403c76>] ? syscall_call+0x7/0xb
    // Code:...  <======== we should ignore everything which isn't call trace
    // RIP  ...
    char *call_trace = strcasestr(oops_buf, "Call Trace:"); /* yes, it must be case-insensitive */
    if (call_trace)
    {
        /* Different architectures have different case
         * and different kind/amount of whitespace after ":" -
         * don't assume there is a single "\n"!
         */
        call_trace += sizeof("Call Trace:")-1;
        call_trace = skip_whitespace(call_trace);
        int i = 0;
        for (;;)
        {
            char *end_line = strchr(call_trace, '\n');
            if (!end_line)
                break;
            char *line = xstrndup(call_trace, end_line - call_trace);

            /* Skip whitespace and "<IRQ>" / "<EOI>" markers */
            char *p = line;
            for (;;)
            {
                p = skip_whitespace(p);
                if (prefixcmp(p, "<NMI>") == 0)
                {
                    p += strlen("<NMI>");
                    continue;
                }
                if (prefixcmp(p, "<IRQ>") == 0)
                {
                    p += strlen("<IRQ>");
                    continue;
                }
                if (prefixcmp(p, "<EOI>") == 0)
                {
                    p += strlen("<EOI>");
                    continue;
                }
                /* Didn't see it in practice,
                 * but code inspection in arch/x86/kernel/dumpstack_64.c
                 * tells me these strings can be there as well:
                 */
                if (prefixcmp(p, "<EOE>") == 0)
                {
                    p += strlen("<EOE>");
                    continue;
                }
                if (prefixcmp(p, "<<EOE>>") == 0)
                {
                    p += strlen("<<EOE>>");
                    continue;
                }
                break;
            }

            char *end_mem_block = strchr(p, ' ');
            if (!end_mem_block)
                goto done; /* no memblock, we are done */
            if (p[0] != '[' || p[1] != '<' || end_mem_block[-2] != '>' || end_mem_block[-1] != ']')
                goto done; /* no memblock, we are done */

            /* skip symbols prefixed with "?" */
            end_mem_block = skip_whitespace(end_mem_block);
            if (only_reliable && end_mem_block && *end_mem_block == '?')
                goto skip_line;
            /* strip out "+off/len" */
            p = strchrnul(end_mem_block, '+');
            /* append "func_name\n" */
            strbuf_append_strf(kernel_bt, "%.*s\n", (int)(p - end_mem_block), end_mem_block);
            if (frame_count > 0 && i == frame_count)
            {
 done:
                free(line);
                break;
            }
            ++i;
 skip_line:
            free(line);
            call_trace = end_line + 1;
        }
        goto gen_hash;
    }

    /* Special-case: if the first line is of form:
     * WARNING: at net/wireless/core.c:614 wdev_cleanup_work+0xe9/0x120 [cfg80211]() (Not tainted)
     * then hash only "file:line func+ofs/len" part.
     */
    if (strncmp(oops_buf, "WARNING: at ", sizeof("WARNING: at ")-1) == 0)
    {
        const char *p = oops_buf + sizeof("WARNING: at ")-1;
        p = strchr(p, ' '); /* skip filename:NNN */
        if (p)
        {
            p = strchrnul(p + 1, ' '); /* skip function_name+0xNN/0xNNN */
            oops_buf += sizeof("WARNING: at ")-1;
            while (oops_buf < p)
                strbuf_append_char(kernel_bt, *oops_buf++);
        }
        goto gen_hash;
    }

    /* If it's an one-line oops (such as MCE), hash the line */
    const char *eol = strchrnul(oops_buf, '\n');
    if (!eol[0] || !eol[1]) /* "line" or "line\n" */
    {
        strbuf_append_strf(kernel_bt, "%.*s", (int)(eol - oops_buf), oops_buf);
    }

 gen_hash: ;
    VERB3 log("bt to hash: '%s'", kernel_bt->buf);

    /* If we failed to find and process bt, we may end up hashing "".
     * Not good. Let user know it via return value.
     */
    int bad = (kernel_bt->len == 0);

    char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, kernel_bt->buf, kernel_bt->len);
    sha1_end(&sha1ctx, hash_bytes);
    strbuf_free(kernel_bt);

    bin2hex(hash_str, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';
    VERB3 log("hash: %s", hash_str);

    return bad;
}

int koops_hash_str(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *oops_buf)
{
    return koops_hash_str_ext(hash_str, oops_buf, /*frame count*/5, /*only reliable*/1);
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

