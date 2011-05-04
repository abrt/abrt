/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "parse_options.h"

static void hash_oops_str(char hash_str[SHA1_RESULT_LEN*2 + 1], char *oops_buf, const char *oops_ptr)
{
    unsigned char old_c;
    unsigned char c = 0;

    char *dst = oops_buf;

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
            {
                *dst++ = *oops_ptr++;
            }
            goto gen_hash;
        }
    }

    while (1)
    {
        old_c = c;
        c = *oops_ptr++;
        if (!c)
            break;
        if (c == '\n')
        {
            // Exclude some lines which have process name - in some oops classes
            // process name is irrelevant and changes with every oops.
            // Lines we filter out:
            // Pid: 8003, comm: Xorg Not tainted (2.6.27.9-159.fc10.i686 #1)
            // Process Xorg (pid: 8003, ti=f0a0c000 task=f2380000 task.ti=f0a0c000)
            if (strncmp(oops_ptr, "Pid: ", 5) == 0
             || strncmp(oops_ptr, "Process ", 8) == 0
            ) {
                while (*oops_ptr && *oops_ptr != '\n')
                    oops_ptr++;
                continue;
            }
        }
        if (!isalnum(old_c))
        {
            if (c >= '0' && c <= '9')
            {
                // Convert all (possibly hex) numbers to just one '0'
                if (c == '0' && *oops_ptr == 'x') // "0xSOMETHING"
                    oops_ptr++;
                while (isxdigit(*oops_ptr))
                    oops_ptr++;
                c = '0';
            }
            else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
            {
                // This *may be* a hex number without 0x prefix: "f0a0c000"
                // Check that it indeed is, and replace with '0'
                const char *oops_ptr2 = oops_ptr;
                while (isxdigit(*oops_ptr2))
                    oops_ptr2++;
                // Does it end in a letter which is not a hex digit?
                // (Example: "abcw" is not a hex number, "abc " is)
                if (!isalpha(*oops_ptr2))
                {
                    // It's "abc " case. Skip the "abc" string
                    oops_ptr = oops_ptr2;
                    c = '0';
                }
                // else: hash the string as-is
            }
        }
        // TODO: Drop call trace tail - in interrupt-driven oopses,
        // everything before interrupt is irrelevant.
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

        *dst++ = c;
    }

 gen_hash: ;

    unsigned char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, oops_buf, dst - oops_buf);
    sha1_end(&sha1ctx, hash_bytes);

    unsigned len = SHA1_RESULT_LEN;
    unsigned char *s = hash_bytes;
    char *d = hash_str;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';
}

int main(int argc, char **argv)
{
    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\b [-vs] -d DIR\n"
        "\n"
        "Calculates and saves UUID and DUPHASH for oops dump directory DIR"
        );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *oops = dd_load_text(dd, FILENAME_BACKTRACE);
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    hash_oops_str(hash_str, oops, oops);
    free(oops);

    dd_save_text(dd, FILENAME_UUID, hash_str);
    dd_save_text(dd, FILENAME_DUPHASH, hash_str);

    dd_close(dd);

    return 0;
}
