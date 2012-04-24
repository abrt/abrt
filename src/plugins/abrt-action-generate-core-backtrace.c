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
#include <btparser/utils.h>
#include <btparser/core-backtrace.h>

/* mostly copypasted from abrt-action-generate-backtrace */
static char *get_gdb_output(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    char *uid_str = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    uid_t uid = -1L;
    if (uid_str)
    {
        uid = xatoi_positive(uid_str);
        free(uid_str);
        if (uid == geteuid())
        {
            uid = -1L; /* no need to setuid/gid if we are already under right uid */
        }
    }
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    char *args[11];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";

    /* NOTE: We used to use additional dirs here, but we don't need them. Maybe
     * we don't need 'set debug-file-directory ...' at all?
     */
    args[2] = (char*)"-ex";
    args[3] = (char*)"set debug-file-directory /usr/lib/debug";

    /* "file BINARY_FILE" is needed, without it gdb cannot properly
     * unwind the stack. Currently the unwind information is located
     * in .eh_frame which is stored only in binary, not in coredump
     * or debuginfo.
     *
     * Fedora GDB does not strictly need it, it will find the binary
     * by its build-id.  But for binaries either without build-id
     * (= built on non-Fedora GCC) or which do not have
     * their debuginfo rpm installed gdb would not find BINARY_FILE
     * so it is still makes sense to supply "file BINARY_FILE".
     *
     * Unfortunately, "file BINARY_FILE" doesn't work well if BINARY_FILE
     * was deleted (as often happens during system updates):
     * gdb uses specified BINARY_FILE
     * even if it is completely unrelated to the coredump.
     * See https://bugzilla.redhat.com/show_bug.cgi?id=525721
     *
     * TODO: check mtimes on COREFILE and BINARY_FILE and not supply
     * BINARY_FILE if it is newer (to at least avoid gdb complaining).
     */
    args[4] = (char*)"-ex";
    args[5] = xasprintf("file %s", executable);
    free(executable);

    args[6] = (char*)"-ex";
    args[7] = xasprintf("core-file %s/"FILENAME_COREDUMP, dump_dir_name);

    args[8] = (char*)"-ex";
    /*args[9] = ... see below */
    args[10] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 2048;
    char *bt = NULL;
    while (1)
    {
        args[9] = xasprintf("backtrace %u", bt_depth);
        bt = exec_vp(args, uid, /*redirect_stderr:*/ 1, /*exec_timeout_sec:*/ 240, NULL);
        free(args[9]);
        if ((bt && strnlen(bt, 256*1024) < 256*1024) || bt_depth <= 32)
        {
            break;
        }

        free(bt);
        bt_depth /= 2;
    }

    free(args[5]);
    free(args[7]);
    return bt;
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
        "& [-v] -d DIR\n"
        "\n"
        "Creates coredump-level backtrace from core dump and corresponding binary"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    if (g_verbose > 1)
        btp_debug_parser = true;

    VERB1 log("Querying gdb for backtrace");
    char *gdb_out = get_gdb_output(dump_dir_name);
    if (gdb_out == NULL)
        return 1;

    /* parse addresses and eventual symbols from the output*/
    GList *backtrace = btp_backtrace_extract_addresses(gdb_out);
    VERB1 log("Extracted %d frames from the backtrace", g_list_length(backtrace));
    free(gdb_out);

    VERB1 log("Running eu-unstrip -n to obatin build ids");
    /* Run eu-unstrip -n to obtain the ids. This should be rewritten to read
     * them directly from the core. */
    char *unstrip_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
    if (unstrip_output == NULL)
        error_msg_and_die("Running eu-unstrip failed");

    /* Get the executable name -- unstrip doesn't know it. */
    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        xfunc_die(); /* dd_opendir already printed error msg */
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    btp_core_assign_build_ids(backtrace, unstrip_output, executable);

    free(executable);
    free(unstrip_output);

    /* Extract address ranges from all the executables in the backtrace*/
    VERB1 log("Computing function fingerprints");
    btp_core_backtrace_fingerprint(backtrace);

    char *formated_backtrace = btp_core_backtrace_fmt(backtrace);

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    dd_save_text(dd, FILENAME_CORE_BACKTRACE, formated_backtrace);
    dd_close(dd);

    free(formated_backtrace);
    return 0;
}
