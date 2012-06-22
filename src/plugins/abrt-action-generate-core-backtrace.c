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
#include <btparser/hash_sha1.h>
#include <btparser/utils.h>
#include <btparser/core-backtrace.h>
#include <btparser/core-backtrace-python.h>
#include <btparser/core-backtrace-oops.h>

static bool raw_fingerprints = false;

static void hash_fingerprints(GList *backtrace)
{
    GList *elem = backtrace;
    struct backtrace_entry *entry;
    char bin_hash[BTP_SHA1_RESULT_BIN_LEN], hash[BTP_SHA1_RESULT_LEN];
    btp_sha1_ctx_t ctx;
    while (elem)
    {
        entry = (struct backtrace_entry *)elem->data;
        if (entry->fingerprint)
        {
            btp_sha1_begin(&ctx);
            btp_sha1_hash(&ctx, entry->fingerprint, strlen(entry->fingerprint));
            btp_sha1_end(&ctx, bin_hash);
            btp_bin2hex(hash, bin_hash, sizeof(bin_hash))[0] = '\0';

            free(entry->fingerprint);
            entry->fingerprint = btp_strndup(hash, sizeof(hash));
        }
        elem = g_list_next(elem);
    }
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
    /* 60 seconds was too limiting on slow machines */
    const int exec_timeout_sec = 240;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] -d DIR\n"
        "\n"
        "Creates coredump-level backtrace from core dump and corresponding binary"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_r = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_BOOL('r', "raw", &raw_fingerprints, _("Do not hash fingerprints")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    if (g_verbose > 1)
        btp_debug_parser = true;

    /* parse addresses and eventual symbols from the output*/
    GList *backtrace;
    /* Get the executable name -- unstrip doesn't know it. */
    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        xfunc_die(); /* dd_opendir already printed error msg */
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    char *txt_backtrace = dd_load_text_ext(dd, FILENAME_BACKTRACE,
                                           DD_FAIL_QUIETLY_ENOENT);
    char *kernel = dd_load_text_ext(dd, FILENAME_KERNEL,
                                    DD_FAIL_QUIETLY_ENOENT |
                                    DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    dd_close(dd);

    if (strcmp(analyzer, "CCpp") == 0)
    {
        VERB1 log("Querying gdb for backtrace");
        char *gdb_out = get_backtrace(dump_dir_name, exec_timeout_sec, "");
        if (gdb_out == NULL)
            xfunc_die();

        backtrace = btp_backtrace_extract_addresses(gdb_out);
        VERB1 log("Extracted %d frames from the backtrace", g_list_length(backtrace));
        free(gdb_out);

        VERB1 log("Running eu-unstrip -n to obatin build ids");
        /* Run eu-unstrip -n to obtain the ids. This should be rewritten to read
         * them directly from the core. */
        char *unstrip_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
        if (unstrip_output == NULL)
            error_msg_and_die("Running eu-unstrip failed");
        btp_core_assign_build_ids(backtrace, unstrip_output, executable);
        free(unstrip_output);

        /* Remove empty lines from the backtrace. */
        GList *loop = backtrace;
        while (loop != NULL)
        {
            struct backtrace_entry *entry = loop->data;
            GList *const tmp_next = g_list_next(loop);
            if (!entry->build_id && !entry->filename && !entry->modname)
                backtrace = g_list_delete_link(backtrace, loop);
            loop = tmp_next;
        }

        /* Extract address ranges from all the executables in the backtrace*/
        VERB1 log("Computing function fingerprints");
        btp_core_backtrace_fingerprint(backtrace);
        if (!raw_fingerprints)
            hash_fingerprints(backtrace);
    }
    else if (strcmp(analyzer, "Python") == 0)
        backtrace = btp_parse_python_backtrace(txt_backtrace);
    else if (strcmp(analyzer, "Kerneloops") == 0 || strcmp(analyzer, "vmcore") == 0)
        backtrace = btp_parse_kerneloops(txt_backtrace, kernel);
    else
        error_msg_and_die(_("Core-backtraces are not supported for '%s'"), analyzer);

    free(txt_backtrace);
    free(kernel);
    free(executable);
    free(analyzer);

    char *formated_backtrace = btp_core_backtrace_fmt(backtrace);

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        xfunc_die();
    dd_save_text(dd, FILENAME_CORE_BACKTRACE, formated_backtrace);
    dd_close(dd);

    free(formated_backtrace);
    btp_core_backtrace_free(backtrace);
    return 0;
}
