/*
    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  Red Hat, Inc.

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


static const char *dump_dir_name = ".";
/* 60 seconds was too limiting on slow machines */
static int exec_timeout_sec = 240;


static char *get_backtrace(const char *dump_dir_name, unsigned timeout_sec, const char *debuginfo_dirs)
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

    /* Let user know what's going on */
    log(_("Generating backtrace"));

    char *args[21];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";

    // when/if gdb supports "set debug-file-directory DIR1:DIR2":
    // (https://bugzilla.redhat.com/show_bug.cgi?id=528668):
    args[2] = (char*)"-ex";
    struct strbuf *set_debug_file_directory = strbuf_new();
    strbuf_append_str(set_debug_file_directory, "set debug-file-directory /usr/lib/debug");
    const char *p = debuginfo_dirs;
    while (1)
    {
        while (*p == ':')
            p++;
        if (*p == '\0')
            break;
        const char *colon_or_nul = strchrnul(p, ':');
        strbuf_append_strf(set_debug_file_directory, ":%.*s/usr/lib/debug", (int)(colon_or_nul - p), p);
        p = colon_or_nul;
    }
    args[3] = strbuf_free_nobuf(set_debug_file_directory);

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
    args[10] = (char*)"-ex";
    args[11] = (char*)"info sharedlib";
    /* glibc's abort() stores its message in __abort_msg variable */
    args[12] = (char*)"-ex";
    args[13] = (char*)"print (char*)__abort_msg";
    args[14] = (char*)"-ex";
    args[15] = (char*)"print (char*)__glib_assert_msg";
    args[16] = (char*)"-ex";
    args[17] = (char*)"info registers";
    args[18] = (char*)"-ex";
    args[19] = (char*)"disassemble";
    args[20] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 2048;
    const char *thread_apply_all = "thread apply all";
    const char *full = " full";
    char *bt = NULL;
    while (1)
    {
        args[9] = xasprintf("%s backtrace %u%s", thread_apply_all, bt_depth, full);
        bt = exec_vp(args, uid, /*redirect_stderr:*/ 1, timeout_sec, NULL);
        free(args[9]);
        if ((bt && strnlen(bt, 256*1024) < 256*1024) || bt_depth <= 32)
        {
            break;
        }

        free(bt);
        bt_depth /= 2;
        if (bt_depth <= 64 && thread_apply_all[0] != '\0')
        {
            /* This program likely has gazillion threads, dont try to bt them all */
            bt_depth = 256;
            thread_apply_all = "";
        }
        if (bt_depth <= 64 && full[0] != '\0')
        {
            /* Looks like there are gigantic local structures or arrays, disable "full" bt */
            bt_depth = 256;
            full = "";
        }
    }

    free(args[3]);
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

    char *i_opt = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [options] -d DIR\n"
        "\n"
        "Analyzes coredump in problem directory DIR, generates and saves backtrace"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_i = 1 << 2,
        OPT_t = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING( 'd', NULL, &dump_dir_name   , "DIR"           , _("Dump directory")),
        OPT_STRING( 'i', NULL, &i_opt           , "DIR1[:DIR2]...", _("Additional debuginfo directories")),
        OPT_INTEGER('t', NULL, &exec_timeout_sec,                   _("Kill gdb if it runs for more than NUM seconds")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    map_string_h *settings = new_map_string();
    if (!load_conf_file(PLUGINS_CONF_DIR"/CCpp.conf", settings, /*skip key w/o values:*/ false))
        error_msg("Can't open '%s'", PLUGINS_CONF_DIR"/CCpp.conf");

    char *value = g_hash_table_lookup(settings, "DebuginfoLocation");
    char *debuginfo_location;
    if (value)
        debuginfo_location = xstrdup(value);
    else
        debuginfo_location = xstrdup(LOCALSTATEDIR"/cache/abrt-di");

    free_map_string(settings);
    char *debuginfo_dirs = NULL;
    if (i_opt)
        debuginfo_dirs = xasprintf("%s:%s", debuginfo_location, i_opt);

    /* Create gdb backtrace */
    char *backtrace = get_backtrace(dump_dir_name, exec_timeout_sec,
            (debuginfo_dirs) ? debuginfo_dirs : debuginfo_location);
    free(debuginfo_location);
    if (!backtrace)
    {
        backtrace = xstrdup("");
        log("get_backtrace() returns NULL, broken core/gdb?");
    }
    free(debuginfo_dirs);
    free_abrt_conf_data();

    /* Store gdb backtrace */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
    dd_close(dd);

    /* Don't be completely silent. gdb run takes a few seconds,
     * it is useful to let user know it (maybe) worked.
     */
    log(_("Backtrace is generated and saved, %u bytes"), (int)strlen(backtrace));
    free(backtrace);

    return 0;
}
