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

void koops_hash_str(char hash_str[SHA1_RESULT_LEN*2 + 1], char *oops_buf, const char *oops_ptr)
{
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
    struct strbuf *kernel_bt = strbuf_new();
    char *call_trace = strstr(oops_buf, "Call Trace");
    if (call_trace)
    {
        call_trace += sizeof("Call Trace\n");
        char *end_line = strchr(call_trace, '\n');
        int i = 0;
        while (end_line && !*end_line)
        {
            char *line = xstrndup(call_trace, end_line - call_trace);

            char *p = skip_whitespace(line);
            char *end_mem_block = strchr(p, ' ');
            if (!end_mem_block)
                error_msg_and_die("no [<mem>] mark");

            end_mem_block = skip_whitespace(end_mem_block);

            char *begin_off_len, *function;

            /* skip symbols prefixed with ? */
            if (end_mem_block && *end_mem_block == '?')
            {
                free(line);
                goto skip_line;
            }
            /* strip out offset +off/len */
            begin_off_len = strchr(end_mem_block, '+');
            if (!begin_off_len)
                error_msg_and_die("'%s'\nno +offset/len at the end of bt", end_mem_block);

            function = xstrndup(end_mem_block, begin_off_len - end_mem_block);
            strbuf_append_strf(kernel_bt, "%s\n", function);
            free(line);
            free(function);
            if (i == 5)
                break;

            ++i;
        skip_line:
            call_trace += end_line - call_trace + 1;
            end_line = strchr(call_trace, '\n');
            if (end_line)
                ++end_line; /* skip \n */
        }
        goto gen_hash;
    }

    /* Special-case: if the first line is of form:
     * WARNING: at net/wireless/core.c:614 wdev_cleanup_work+0xe9/0x120 [cfg80211]() (Not tainted)
     * then hash only "file:line func+ofs/len" part.
     */
    if (strncmp(oops_ptr, "WARNING: at ", sizeof("WARNING: at ")-1) == 0)
    {
        const char *p = oops_ptr + sizeof("WARNING: at ")-1;
        p = strchr(p, ' '); /* skip filename:NNN */
        if (p)
        {
            p = strchrnul(p + 1, ' '); /* skip function_name+0xNN/0xNNN */
            oops_ptr += sizeof("WARNING: at ")-1;
            while (oops_ptr < p)
                strbuf_append_char(kernel_bt, *oops_ptr++);
        }
    }

gen_hash: ;

    char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, kernel_bt->buf, kernel_bt->len);
    sha1_end(&sha1ctx, hash_bytes);
    strbuf_free(kernel_bt);

    bin2hex(hash_str, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';
    VERB3 log("hash: %s", hash_str);
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

