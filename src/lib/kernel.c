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

#include <regex.h>

#define _GNU_SOURCE 1 /* for strcasestr */
#include "libabrt.h"

/* Used to be 100, but some MCE oopses are short:
 * "CPU 0: Machine Check Exception: 0000000000000007"
 */
#define SANE_MIN_OOPS_LEN 30

static void record_oops(GList **oops_list, const struct abrt_koops_line_info* lines_info, int oopsstart, int oopsend)
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
                        xasprintf("%s\n%s", (version ? version : ""), oops)
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

    VERB3 if (rv == 0) log_warning("Dropped oops: too short");
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
    "Kernel panic - not syncing:",
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
    "invalid opcode",
    "alignment check:",
    "stack segment:",
    "fpu exception:",
    "simd exception:",
    "iret exception:",

    /* Termination */
    NULL
};

static const char *const s_koops_suspicious_strings_blacklist[] = {
    /* "BUG:" and "DEBUG:" overlaps, we don't want to recognize DEBUG messages as BUG */
    "DEBUG:",

    /* Termination */
    NULL
};

static bool suspicious_line(const char *line)
{
    const char *const *str = s_koops_suspicious_strings;
    for ( ; *str; ++str)
        if (strstr(line, *str))
            break;

    if (!*str)
        return false;

    str = s_koops_suspicious_strings_blacklist;
    for ( ; *str; ++str)
        if (strstr(line, *str))
           break;

    return !*str;
}

void koops_print_suspicious_strings(void)
{
    koops_print_suspicious_strings_filtered(NULL);
}

GList *koops_suspicious_strings_list(void)
{
    GList *strings = NULL;
    for (const char *const *str = s_koops_suspicious_strings; *str; ++str)
        strings = g_list_prepend(strings, (gpointer)*str);

    return strings;
}

GList *koops_suspicious_strings_blacklist(void)
{
    GList *strings = NULL;
    for (const char *const *str = s_koops_suspicious_strings_blacklist; *str; ++str)
        strings = g_list_prepend(strings, (gpointer)*str);

    return strings;
}

static bool match_any(const regex_t **res, const char *str)
{
    for (const regex_t **r = res; *r != NULL; ++r)
    {
        /* Regular expressions compiled with REG_NOSUB */
        const int reti = regexec(*r, str, 0, NULL, 0);
        if (reti == 0)
            return true;
        else if (reti != REG_NOMATCH)
        {
            char msgbuf[100];
            regerror(reti, *r, msgbuf, sizeof(msgbuf));
            error_msg_and_die("Regex match failed: %s", msgbuf);
        }
    }

    return false;
}

void koops_print_suspicious_strings_filtered(const regex_t **filterout)
{
    for (const char *const *str = s_koops_suspicious_strings; *str; ++str)
    {
        if (filterout == NULL || !match_any(filterout, *str))
            puts(*str);
    }
}


void koops_line_skip_jiffies(const char **c)
{
    /* remove jiffies time stamp counter if present
     * jiffies are unsigned long, so it can be 2^64 long, which is
     * 20 decimal digits
     */
    if (**c == '[')
    {
        const char *c2 = strchr(*c, '.');
        const char *c3 = strchr(*c, ']');
        if (c2 && c3 && (c2 < c3) && (c3-*c) < 21)
        {
            *c = c3 + 1;
            if (**c == ' ')
                (*c)++;
        }
    }
}

int koops_line_skip_level(const char **c)
{
    int linelevel = 0;
    if (**c == '<')
    {
        const char *ptr = *c + 1;
        while (isdigit(*ptr))
            ++ptr;

        if (*ptr == '>' && (ptr - *c > 1))
        {
            const char *const bck = ptr + 1;
            unsigned exp = 1;
            while (--ptr != *c)
            {
                linelevel += (*ptr - '0') * exp;
                exp *= 10;
            }
            *c = bck;
        }
    }

    return linelevel;
}

void koops_extract_oopses(GList **oops_list, char *buffer, size_t buflen)
{
    char *c;
    int linecount = 0;
    int lines_info_size = 0;
    struct abrt_koops_line_info *lines_info = NULL;

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
                    log_debug("Found our marker at line %d", linecount);
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

        /* store and remove kernel log level */
        linelevel = koops_line_skip_level((const char **)&c);
        koops_line_skip_jiffies((const char **)&c);

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

    koops_extract_oopses_from_lines(oops_list, lines_info, lines_info_size);
    free(lines_info);
}

void koops_extract_oopses_from_lines(GList **oops_list, const struct abrt_koops_line_info *lines_info, int lines_info_size)
{
    /* Analyze lines */

    int i;
    char prevlevel = 0;
    int oopsstart = -1;
    int inbacktrace = 0;
    regex_t arm_regex;
    int arm_regex_rc = 0;

    /* ARM backtrace regex, match a string similar to r7:df912310 */
    arm_regex_rc = regcomp(&arm_regex, "r[[:digit:]]{1,}:[a-f[:digit:]]{8}", REG_EXTENDED | REG_NOSUB);

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
            if (suspicious_line(curline))
                oopsstart = i;

            if (oopsstart >= 0)
            {
                /* debug information */
                log_debug("Found oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
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
            /* Fatal MCE's have a few lines of useful information between
             * first "Machine check exception:" line and the final "Kernel panic"
             * line. Such oops, of course, is only detectable in kdumps (tested)
             * or possibly pstore-saved logs (I did not try this yet).
             * In order to capture all these lines, we treat final line
             * as "backtrace" (which is admittedly a hack):
             */
            if (strstr(curline, "Kernel panic - not syncing:") && strcasestr(curline, "Machine check"))
                inbacktrace = 1;
            else
            if (strnlen(curline, 9) > 8
             && (  (curline[0] == '(' && curline[1] == '[' && curline[2] == '<')
                || (curline[0] == '[' && curline[1] == '<'))
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
            /* example s390: "([<ffffffffa006c156>] 0xdeadbeaf)" */
            if ((curline[0] != '[' && (curline[0] != '(' || curline[1] != '['))
             && !strstr(curline, "] [")
             && !strstr(curline, "--- Exception")
             && !strstr(curline, "LR =")
             && !strstr(curline, "<#DF>")
             && !strstr(curline, "<IRQ>")
             && !strstr(curline, "<EOI>")
             && !strstr(curline, "<NMI>")
             && !strstr(curline, "<<EOE>>")
             && !strstr(curline, "Comm:")
             && !strstr(curline, "Hardware name:")
             && !strstr(curline, "Backtrace:")
             && strncmp(curline, "Code: ", 6) != 0
             && strncmp(curline, "RIP ", 4) != 0
             && strncmp(curline, "RSP ", 4) != 0
             /* s390 Call Trace ends with 'Last Breaking-Event-Address:'
              * which is followed by a single frame */
             && strncmp(curline, "Last Breaking-Event-Address:", strlen("Last Breaking-Event-Address:")) != 0
             /* ARM dumps registers intertwined with the backtrace */
             && (arm_regex_rc == 0 ? regexec(&arm_regex, curline, 0, NULL, 0) == REG_NOMATCH : 1)
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
            /* if a new oops starts, this one has ended */
            else if (suspicious_line(curline))
                oopsend = i-1;

            if (oopsend <= i)
            {
                log_debug("End of oops at line %d (%d): '%s'", oopsend, i, lines_info[oopsend].ptr);
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
                log_debug("Dropped oops, too long");
                continue;
            }
            if (!inbacktrace && i - oopsstart > 40)
            {
                /* Used to drop oopses w/o backtraces, but some of them
                 * (MCEs, for example) don't have backtrace yet we still want to file them.
                 */
                log_debug("One-line oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
                record_oops(oops_list, lines_info, oopsstart, oopsstart);
                /*inbacktrace = 0; - already is */
                oopsstart = -1;
                continue;
            }
        }
    } /* while (i < lines_info_size) */

    regfree(&arm_regex);

    /* process last oops if we have one */
    if (oopsstart >= 0)
    {
        if (inbacktrace)
        {
            int oopsend = i-1;
            log_debug("End of oops at line %d (end of file): '%s'", oopsend, lines_info[oopsend].ptr);
            record_oops(oops_list, lines_info, oopsstart, oopsend);
        }
        else
        {
            log_debug("One-line oops at line %d: '%s'", oopsstart, lines_info[oopsstart].ptr);
            record_oops(oops_list, lines_info, oopsstart, oopsstart);
        }
    }
}

int koops_hash_str_ext(char result[SHA1_RESULT_LEN*2 + 1], const char *oops_buf, int frame_count, int duphash_flags)
{
    char *hash_str = NULL, *error = NULL;
    int bad = 0;

    struct sr_stacktrace *stacktrace = sr_stacktrace_parse(SR_REPORT_KERNELOOPS,
                                                           oops_buf, &error);
    if (!stacktrace)
    {
        log_debug("Failed to parse koops: %s", error);
        free(error);
        bad = 1;
        goto end;
    }

    struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
    {
        log_debug("Failed to find crash thread");
        bad = 1;
        goto end;
    }

    if (g_verbose >= 3)
    {
        hash_str = sr_thread_get_duphash(thread, frame_count, NULL,
                                         duphash_flags|SR_DUPHASH_NOHASH);
        if (hash_str)
            log_warning("Generating duphash: '%s'", hash_str);
        else
            log_warning("Nothing useful for duphash");


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

int koops_hash_str(char result[SHA1_RESULT_LEN*2 + 1], const char *oops_buf)
{
    const int frame_count = 6;
    const int duphash_flags = SR_DUPHASH_NONORMALIZE|SR_DUPHASH_KOOPS_COMPAT;
    return koops_hash_str_ext(result, oops_buf, frame_count, duphash_flags);
}

char *koops_extract_version(const char *linepointer)
{
    if (strstr(linepointer, "Pid")
     || strstr(linepointer, "comm")
     || strstr(linepointer, "CPU")
     || strstr(linepointer, "REGS")
     || strstr(linepointer, "EFLAGS")
    ) {
        /* "(4.7.0-2.x86_64.fc25) #"    */
        /* " 4.7.0-2.x86_64.fc25 #"     */
        /* " 2.6.3.4.5-2.x86_64.fc22 #" */
        const char *regexp = "([ \\(]|kernel-)([0-9]+\\.[0-9]+\\.[0-9]+(\\.[^.-]+)*-[^ \\)]+)\\)? #";
        regex_t re;
        int r = regcomp(&re, regexp, REG_EXTENDED);
        if (r != 0)
        {
            char buf[LINE_MAX];
            regerror(r, &re, buf, sizeof(buf));
            error_msg("BUG: invalid kernel version regexp: %s", buf);
            return NULL;
        }

        regmatch_t matchptr[3];
        r = regexec(&re, linepointer, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (r != 0)
        {
            if (r != REG_NOMATCH)
            {
                char buf[LINE_MAX];
                regerror(r, &re, buf, sizeof(buf));
                error_msg("BUG: kernel version regexp failed: %s", buf);
            }
            else
            {
                log_debug("A kernel version candidate line didn't match kernel oops regexp:");
                log_debug("\t'%s'", linepointer);
            }

            regfree(&re);
            return NULL;
        }

        /* 0: entire string */
        /* 1: version prefix */
        /* 2: version string */
        const regmatch_t *const ver = matchptr + 2;
        char *ret = xstrndup(linepointer + ver->rm_so, ver->rm_eo - ver->rm_so);

        regfree(&re);
        return ret;
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
 *  'O' - Out-of-tree module has been loaded.
 *  'E' - Unsigned module has been loaded.
 *  'L' - A soft lockup has previously occurred.
 *  'K' - Kernel has been live patched.
 *
 * Compatibility flags from older versions and downstream sources:
 *  'H' - Hardware is unsupported.
 *  'T' - Tech_preview
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
    /* example of flags: 'Tainted: G    B       ' */
    char *tainted = strstr(kernel_bt, "Tainted: ");
    if (!tainted)
        return NULL;

    tainted += strlen("Tainted: ");
    /* 17 + 2 == current count of known flags */
    /* http://git.kernel.org/?p=linux/kernel/git/torvalds/linux.git;a=blob_plain;f=kernel/panic.c;hb=HEAD */
    /* 26 the maximal sane count of flags because of alphabet limits */
    unsigned sz = 26 + 1;
    unsigned cnt = 0;
    char *tnt = xmalloc(sz);

    for (;;)
    {
        if (tainted[0] >= 'A' && tainted[0] <= 'Z')
        {
            if (cnt == sz - 1)
            {   /* this should not happen but */
                /* I guess, it's a bit better approach than simple failure */
                sz <<= 1;
                tnt = xrealloc(tnt, sizeof(char) * sz);
            }

            tnt[cnt] = tainted[0];
            ++cnt;
        }
        else if (tainted[0] != ' ')
            break;

        ++tainted;
    }

    if (cnt == 0)
    {   /* this should not happen
         * cnt eq 0 means that a tainted string contains only spaces */
        free(tnt);
        return NULL;
    }

    tnt[cnt] = '\0';
    return tnt;
}

static const char *const tnts_long[] = {
    /* A */ "ACPI table overridden.",
    /* B */ "System has hit bad_page.",
    /* C */ "Modules from drivers/staging are loaded.",
    /* D */ "Kernel has oopsed before",
    /* E */ "Unsigned module has been loaded.",
    /* F */ "Module has been forcibly loaded.",
            /* We don't want to be more descriptive about G flag */
    /* G */ NULL, /* "Proprietary module has not been loaded." */
    /* H */ NULL,
    /* I */ "Working around severe firmware bug.",
    /* J */ NULL,
    /* K */ "Kernel has been live patched.",
    /* L */ "A soft lockup has previously occurred.",
    /* M */ "System experienced a machine check exception.",
    /* N */ NULL,
    /* O */ "Out-of-tree module has been loaded.",
    /* P */ "Proprietary module has been loaded.",
    /* Q */ NULL,
    /* R */ "User forced a module unload.",
    /* S */ "SMP with CPUs not designed for SMP.",
    /* T */ NULL,
    /* U */ "Userspace-defined naughtiness.",
    /* V */ NULL,
    /* W */ "Taint on warning.",
    /* X */ NULL,
    /* Y */ NULL,
    /* Z */ NULL,
};

char *kernel_tainted_long(const char *tainted_short)
{
    struct strbuf *tnt_long = strbuf_new();
    while (tainted_short[0] != '\0')
    {
        const int tnt_index = tainted_short[0] - 'A';
        if (tnt_index >= 0 && tnt_index <= 'Z' - 'A')
        {
            const char *const txt = tnts_long[tnt_index];
            if (txt)
                strbuf_append_strf(tnt_long, "%c - %s\n", tainted_short[0], txt);
        }

        ++tainted_short;
    }

    return strbuf_free_nobuf(tnt_long);
}

