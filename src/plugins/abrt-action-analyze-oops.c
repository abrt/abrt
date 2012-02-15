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
#include "libabrt.h"

static void hash_oops_str(char hash_str[SHA1_RESULT_LEN*2 + 1], char *oops_buf, const char *oops_ptr)
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
                goto skip_line;

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
            free(line);
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

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-vs] -d DIR\n"
        "\n"
        "Calculates and saves UUID and DUPHASH for oops problem directory DIR"
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
