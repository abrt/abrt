/*
    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "../btparser/backtrace.h"
#include "../btparser/frame.h"
#include "../btparser/location.h"
#include "parse_options.h"


#define DEBUGINFO_CACHE_DIR     LOCALSTATEDIR"/cache/abrt-di"

static const char *dump_dir_name = ".";
static const char *debuginfo_dirs;
static int exec_timeout_sec = 60;


static void create_hash(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *pInput)
{
    unsigned len;
    unsigned char hash2[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;

    sha1_begin(&sha1ctx);
    sha1_hash(pInput, strlen(pInput), &sha1ctx);
    sha1_end(hash2, &sha1ctx);
    len = SHA1_RESULT_LEN;

    char *d = hash_str;
    unsigned char *s = hash2;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';
    //log("hash2:%s str:'%s'", hash_str, pInput);
}

/**
 *
 * @param[out] status See `man 2 wait` for status information.
 * @return Malloc'ed string
 */
static char* exec_vp(char **args, uid_t uid, int redirect_stderr, int *status)
{
    /* Nuke everything which may make setlocale() switch to non-POSIX locale:
     * we need to avoid having gdb output in some obscure language.
     */
    static const char *const unsetenv_vec[] = {
        "LANG",
        "LC_ALL",
        "LC_COLLATE",
        "LC_CTYPE",
        "LC_MESSAGES",
        "LC_MONETARY",
        "LC_NUMERIC",
        "LC_TIME",
        NULL
    };

    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_SETGUID | EXECFLG_SETSID | EXECFLG_QUIET;
    if (redirect_stderr)
        flags |= EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    int pipeout[2];
    pid_t child = fork_execv_on_steroids(flags, args, pipeout, (char**)unsetenv_vec, /*dir:*/ NULL, uid);

    /* We use this function to run gdb and unstrip. Bugs in gdb or corrupted
     * coredumps were observed to cause gdb to enter infinite loop.
     * Therefore we have a (largish) timeout, after which we kill the child.
     */
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + exec_timeout_sec;

    struct strbuf *buf_out = strbuf_new();

    while (1)
    {
        int timeout = endtime - t;
        if (timeout < 0)
        {
            kill(child, SIGKILL);
            strbuf_append_strf(buf_out, "\nTimeout exceeded: %u seconds, killing %s\n", exec_timeout_sec, args[0]);
            break;
        }

        /* We don't check poll result - checking read result is enough */
        struct pollfd pfd;
        pfd.fd = pipeout[0];
        pfd.events = POLLIN;
        poll(&pfd, 1, timeout * 1000);

        char buff[1024];
        int r = read(pipeout[0], buff, sizeof(buff) - 1);
        if (r <= 0)
            break;
        buff[r] = '\0';
        strbuf_append_str(buf_out, buff);
        t = time(NULL);
    }
    close(pipeout[0]);

    /* Prevent having zombie child process, and maybe collect status
     * (note that status == NULL is ok too) */
    waitpid(child, status, 0);

    return strbuf_free_nobuf(buf_out);
}

static char *get_backtrace(struct dump_dir *dd)
{
    char *uid_str = dd_load_text(dd, CD_UID);
    uid_t uid = xatoi_u(uid_str);
    free(uid_str);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    // Workaround for
    // http://sourceware.org/bugzilla/show_bug.cgi?id=9622
    unsetenv("TERM");
    // This is not necessary
    //putenv((char*)"TERM=dumb");

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
        bt = exec_vp(args, uid, /*redirect_stderr:*/ 1, NULL);
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

static char *i_opt;
static const char abrt_action_generage_backtrace_usage[] = "abrt-action-generate-backtrace [options] -d DIR";
enum {
    OPT_v = 1 << 0,
    OPT_d = 1 << 1,
    OPT_i = 1 << 2,
    OPT_t = 1 << 3,
    OPT_s = 1 << 4,
};
/* Keep enum above and order of options below in sync! */
static struct options abrt_action_generate_backtrace_options[] = {
    OPT__VERBOSE(&g_verbose),
    OPT_STRING( 'd', NULL, &dump_dir_name, "DIR", "Crash dump directory"),
    OPT_STRING( 'i', NULL, &i_opt, "dir1[:dir2]...", "Additional debuginfo directories"),
    OPT_INTEGER('t', NULL, &exec_timeout_sec, "Kill gdb if it runs for more than N seconds"),
    OPT_BOOL(   's', NULL, NULL, "Log to syslog"),
    OPT_END()
};

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    unsigned opts = parse_opts(argc, argv, abrt_action_generate_backtrace_options,
                           abrt_action_generage_backtrace_usage);

    debuginfo_dirs = DEBUGINFO_CACHE_DIR;
    if (i_opt)
    {
        debuginfo_dirs = xasprintf("%s:%s", DEBUGINFO_CACHE_DIR, i_opt);
    }

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));
    msg_prefix = xasprintf("abrt-action-generate-backtrace[%u]", getpid());

    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    struct dump_dir *dd = dd_init();
    if (!dd_opendir(dd, dump_dir_name, DD_CLOSE_ON_OPEN_ERR))
        return 1;

    char *package = dd_load_text(dd, FILENAME_PACKAGE);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);

    /* Create and store backtrace */
    /* NB: get_backtrace() closes dd */
    char *backtrace_str = get_backtrace(dd);
    if (!backtrace_str)
    {
        backtrace_str = xstrdup("");
        VERB3 log("get_backtrace() returns NULL, broken core/gdb?");
    }

    dd = dd_init();
    if (!dd_opendir(dd, dump_dir_name, DD_CLOSE_ON_OPEN_ERR))
        return 1;

    dd_save_text(dd, FILENAME_BACKTRACE, backtrace_str);

    /* Compute and store backtrace hash. */
    struct btp_location location;
    btp_location_init(&location);
    char *backtrace_str_ptr = backtrace_str;
    struct btp_backtrace *backtrace = btp_backtrace_parse(&backtrace_str_ptr, &location);
    if (!backtrace)
    {
        VERB1 log(_("Backtrace parsing failed for %s"), dump_dir_name);
        VERB1 log("%d:%d: %s", location.line, location.column, location.message);
        /* If the parser failed compute the UUID from the executable
           and package only.  This is not supposed to happen often.
           Do not store the rating, as we do not know how good the
           backtrace is. */
        struct strbuf *emptybt = strbuf_new();
        strbuf_prepend_str(emptybt, executable);
        strbuf_prepend_str(emptybt, package);
        char hash_str[SHA1_RESULT_LEN*2 + 1];
        create_hash(hash_str, emptybt->buf);
        dd_save_text(dd, FILENAME_DUPHASH, hash_str);

        strbuf_free(emptybt);
        free(backtrace_str);
        free(package);
        free(executable);
        dd_close(dd);
        return 2;
    }
    free(backtrace_str);

    /* Compute duplication hash. */
    char *str_hash_core = btp_backtrace_get_duplication_hash(backtrace);
    struct strbuf *str_hash = strbuf_new();
    strbuf_append_str(str_hash, package);
    strbuf_append_str(str_hash, executable);
    strbuf_append_str(str_hash, str_hash_core);
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    create_hash(hash_str, str_hash->buf);
    dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    strbuf_free(str_hash);
    free(str_hash_core);

    /* Compute the backtrace rating. */
    float quality = btp_backtrace_quality_complex(backtrace);
    const char *rating;
    if (quality < 0.6f)
        rating = "0";
    else if (quality < 0.7f)
        rating = "1";
    else if (quality < 0.8f)
        rating = "2";
    else if (quality < 0.9f)
        rating = "3";
    else
        rating = "4";
    dd_save_text(dd, FILENAME_RATING, rating);

    /* Get the function name from the crash frame. */
    struct btp_frame *crash_frame = btp_backtrace_get_crash_frame(backtrace);
    if (crash_frame)
     {
        if (crash_frame->function_name &&
            0 != strcmp(crash_frame->function_name, "??"))
        {
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, crash_frame->function_name);
        }
        btp_frame_free(crash_frame);
     }
    btp_backtrace_free(backtrace);
    dd_close(dd);

    free(executable);
    free(package);

    return 0;
}
