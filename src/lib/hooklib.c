/*
    Copyright (C) 2009 RedHat inc.

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
#include <sys/statvfs.h>
#include "internal_libabrt.h"

int low_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location)
{
    struct statvfs vfs;
    if (statvfs(dump_location, &vfs) != 0)
    {
        perror_msg("statvfs('%s')", dump_location);
        return 0;
    }

    /* Check that at least MaxCrashReportsSize/4 MBs are free */

    /* fs_free_mb_x4 ~= vfs.f_bfree * vfs.f_bsize * 4, expressed in MBytes.
     * Need to neither overflow nor round f_bfree down too much. */
    unsigned long fs_free_mb_x4 = ((unsigned long long)vfs.f_bfree / (1024/4)) * vfs.f_bsize / 1024;
    if (fs_free_mb_x4 < setting_MaxCrashReportsSize)
    {
        error_msg("Only %luMiB is available on %s",
                          fs_free_mb_x4 / 4, dump_location);
        return 1;
    }
    return 0;
}

/* rhbz#539551: "abrt going crazy when crashing process is respawned".
 * Check total size of problem dirs, if it overflows,
 * delete oldest/biggest dirs.
 */
void trim_problem_dirs(const char *dirname, double cap_size, const char *exclude_path)
{
    const char *excluded_basename = NULL;
    if (exclude_path)
    {
        unsigned len_dirname = strlen(dirname);
        /* Trim trailing '/'s, but dont trim name "/" to "" */
        while (len_dirname > 1 && dirname[len_dirname-1] == '/')
            len_dirname--;
        if (strncmp(dirname, exclude_path, len_dirname) == 0
         && exclude_path[len_dirname] == '/'
        ) {
            /* exclude_path is "dirname/something" */
            excluded_basename = exclude_path + len_dirname + 1;
        }
    }
    log_debug("excluded_basename:'%s'", excluded_basename);

    int count = 20;
    while (--count >= 0)
    {
        /* We exclude our own dir from candidates for deletion (3rd param): */
        char *worst_basename = NULL;
        double cur_size = get_dirsize_find_largest_dir(dirname, &worst_basename, excluded_basename);
        if (cur_size <= cap_size || !worst_basename)
        {
            log_info("cur_size:%.0f cap_size:%.0f, no (more) trimming", cur_size, cap_size);
            free(worst_basename);
            break;
        }
        log("%s is %.0f bytes (more than %.0fMiB), deleting '%s'",
                dirname, cur_size, cap_size / (1024*1024), worst_basename);
        char *d = concat_path_file(dirname, worst_basename);
        free(worst_basename);
        delete_dump_dir(d);
        free(d);
    }
}

/**
 *
 * @param[out] status See `man 2 wait` for status information.
 * @return Malloc'ed string
 */
static char* exec_vp(char **args, int redirect_stderr, int exec_timeout_sec, int *status)
{
    /* Nuke everything which may make setlocale() switch to non-POSIX locale:
     * we need to avoid having gdb output in some obscure language.
     */
    static const char *const env_vec[] = {
        "LANG",
        "LC_ALL",
        "LC_COLLATE",
        "LC_CTYPE",
        "LC_MESSAGES",
        "LC_MONETARY",
        "LC_NUMERIC",
        "LC_TIME",
        /* Workaround for
         * http://sourceware.org/bugzilla/show_bug.cgi?id=9622
         * (gdb emitting ESC sequences even with -batch)
         */
        "TERM",
        NULL
    };

    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_SETSID | EXECFLG_QUIET;
    if (redirect_stderr)
        flags |= EXECFLG_ERR2OUT;
    VERB1 flags &= ~EXECFLG_QUIET;

    int pipeout[2];
    pid_t child = fork_execv_on_steroids(flags, args, pipeout, (char**)env_vec, /*dir:*/ NULL, /*uid(unused):*/ 0);

    /* We use this function to run gdb and unstrip. Bugs in gdb or corrupted
     * coredumps were observed to cause gdb to enter infinite loop.
     * Therefore we have a (largish) timeout, after which we kill the child.
     */
    ndelay_on(pipeout[0]);
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + exec_timeout_sec;
    struct strbuf *buf_out = strbuf_new();
    while (1)
    {
        int timeout = endtime - t;
        if (timeout < 0)
        {
            kill(child, SIGKILL);
            strbuf_append_strf(buf_out, "\n"
                        "Timeout exceeded: %u seconds, killing %s.\n"
                        "Looks like gdb hung while generating backtrace.\n"
                        "This may be a bug in gdb. Consider submitting a bug report to gdb developers.\n"
                        "Please attach coredump from this crash to the bug report if you do.\n",
                        exec_timeout_sec, args[0]
            );
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
        {
            /* I did see EAGAIN happening here */
            if (r < 0 && errno == EAGAIN)
                goto next;
            break;
        }
        buff[r] = '\0';
        strbuf_append_str(buf_out, buff);
 next:
        t = time(NULL);
    }
    close(pipeout[0]);

    /* Prevent having zombie child process, and maybe collect status
     * (note that status == NULL is ok too) */
    safe_waitpid(child, status, 0);

    return strbuf_free_nobuf(buf_out);
}

char *run_unstrip_n(const char *dump_dir_name, unsigned timeout_sec)
{
    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_SETSID | EXECFLG_QUIET;
    VERB1 flags &= ~EXECFLG_QUIET;
    int pipeout[2];
    char* args[4];
    args[0] = (char*)"eu-unstrip";
    args[1] = xasprintf("--core=%s/"FILENAME_COREDUMP, dump_dir_name);
    args[2] = (char*)"-n";
    args[3] = NULL;
    pid_t child = fork_execv_on_steroids(flags, args, pipeout, /*env_vec:*/ NULL, /*dir:*/ NULL, /*uid(unused):*/ 0);
    free(args[1]);

    /* Bugs in unstrip or corrupted coredumps can cause it to enter infinite loop.
     * Therefore we have a (largish) timeout, after which we kill the child.
     */
    ndelay_on(pipeout[0]);
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + timeout_sec;
    struct strbuf *buf_out = strbuf_new();
    while (1)
    {
        int timeout = endtime - t;
        if (timeout < 0)
        {
            kill(child, SIGKILL);
            strbuf_free(buf_out);
            buf_out = NULL;
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
        {
            /* I did see EAGAIN happening here */
            if (r < 0 && errno == EAGAIN)
                goto next;
            break;
        }
        buff[r] = '\0';
        strbuf_append_str(buf_out, buff);
 next:
        t = time(NULL);
    }
    close(pipeout[0]);

    /* Prevent having zombie child process */
    int status;
    safe_waitpid(child, &status, 0);

    if (status != 0 || buf_out == NULL)
    {
        /* unstrip didnt exit with exit code 0, or we timed out */
        strbuf_free(buf_out);
        return NULL;
    }

    return strbuf_free_nobuf(buf_out);
}

char *get_backtrace(const char *dump_dir_name, unsigned timeout_sec, const char *debuginfo_dirs)
{
    INITIALIZE_LIBABRT();

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    /* Let user know what's going on */
    log(_("Generating backtrace"));

    unsigned i = 0;
    char *args[25];
    args[i++] = (char*)"gdb";
    args[i++] = (char*)"-batch";
    struct strbuf *set_debug_file_directory = strbuf_new();
    unsigned auto_load_base_index = 0;
    if(debuginfo_dirs == NULL)
    {
        // set non-existent debug file directory to prevent resolving
        // function names - we need offsets for core backtrace.
        strbuf_append_str(set_debug_file_directory, "set debug-file-directory /");
    }
    else
    {
        strbuf_append_str(set_debug_file_directory, "set debug-file-directory /usr/lib/debug");

        struct strbuf *debug_directories = strbuf_new();
        const char *p = debuginfo_dirs;
        while (1)
        {
            while (*p == ':')
                p++;
            if (*p == '\0')
                break;
            const char *colon_or_nul = strchrnul(p, ':');
            strbuf_append_strf(debug_directories, "%s%.*s/usr/lib/debug", (debug_directories->len == 0 ? "" : ":"),
                                                                          (int)(colon_or_nul - p), p);
            p = colon_or_nul;
        }

        strbuf_append_strf(set_debug_file_directory, ":%s", debug_directories->buf);

        args[i++] = (char*)"-iex";
        auto_load_base_index = i;
        args[i++] = xasprintf("add-auto-load-safe-path %s", debug_directories->buf);
        args[i++] = (char*)"-iex";
        args[i++] = xasprintf("add-auto-load-scripts-directory %s", debug_directories->buf);

        strbuf_free(debug_directories);
    }

    args[i++] = (char*)"-ex";
    const unsigned debug_dir_cmd_index = i++;
    args[debug_dir_cmd_index] = strbuf_free_nobuf(set_debug_file_directory);

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
    args[i++] = (char*)"-ex";
    const unsigned file_cmd_index = i++;
    args[file_cmd_index] = xasprintf("file %s", executable);
    free(executable);

    args[i++] = (char*)"-ex";
    const unsigned core_cmd_index = i++;
    args[core_cmd_index] = xasprintf("core-file %s/"FILENAME_COREDUMP, dump_dir_name);

    args[i++] = (char*)"-ex";
    const unsigned bt_cmd_index = i++;
    /*args[9] = ... see below */
    args[i++] = (char*)"-ex";
    args[i++] = (char*)"info sharedlib";
    /* glibc's abort() stores its message in __abort_msg variable */
    args[i++] = (char*)"-ex";
    args[i++] = (char*)"print (char*)__abort_msg";
    args[i++] = (char*)"-ex";
    args[i++] = (char*)"print (char*)__glib_assert_msg";
    args[i++] = (char*)"-ex";
    args[i++] = (char*)"info all-registers";
    args[i++] = (char*)"-ex";
    const unsigned dis_cmd_index = i++;
    args[dis_cmd_index] = (char*)"disassemble";
    args[i++] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 1024;
    const char *thread_apply_all = "thread apply all";
    const char *full = " full";
    char *bt = NULL;
    while (1)
    {
        args[bt_cmd_index] = xasprintf("%s backtrace %u%s", thread_apply_all, bt_depth, full);
        bt = exec_vp(args, /*redirect_stderr:*/ 1, timeout_sec, NULL);
        free(args[bt_cmd_index]);
        if ((bt && strnlen(bt, 256*1024) < 256*1024) || bt_depth <= 32)
        {
            break;
        }

        bt_depth /= 2;
        if (bt)
            log("Backtrace is too big (%u bytes), reducing depth to %u",
                        (unsigned)strlen(bt), bt_depth);
        else
            /* (NB: in fact, current impl. of exec_vp() never returns NULL) */
            log("Failed to generate backtrace, reducing depth to %u",
                        bt_depth);
        free(bt);

        /* Replace -ex disassemble (which disasms entire function $pc points to)
         * to a version which analyzes limited, small patch of code around $pc.
         * (Users reported a case where bare "disassemble" attempted to process
         * entire .bss).
         * TODO: what if "$pc-N" underflows? in my test, this happens:
         * Dump of assembler code from 0xfffffffffffffff0 to 0x30:
         * End of assembler dump.
         * (IOW: "empty" dump)
         */
        args[dis_cmd_index] = (char*)"disassemble $pc-20, $pc+64";

        if (bt_depth <= 64 && thread_apply_all[0] != '\0')
        {
            /* This program likely has gazillion threads, dont try to bt them all */
            bt_depth = 128;
            thread_apply_all = "";
        }
        if (bt_depth <= 64 && full[0] != '\0')
        {
            /* Looks like there are gigantic local structures or arrays, disable "full" bt */
            bt_depth = 128;
            full = "";
        }
    }

    if (auto_load_base_index > 0)
    {
        free(args[auto_load_base_index]);
        free(args[auto_load_base_index + 2]);
    }

    free(args[debug_dir_cmd_index]);
    free(args[file_cmd_index]);
    free(args[core_cmd_index]);
    return bt;
}

char* problem_data_save(problem_data_t *pd)
{
    load_abrt_conf();

    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, g_settings_dump_location);

    char *problem_id = NULL;
    if (dd)
    {
        problem_id = xstrdup(dd->dd_dirname);
        dd_close(dd);
    }

    log_info("problem id: '%s'", problem_id);
    return problem_id;
}

int dump_suid_policy()
{
    /*
     - values are:
       0 - don't dump suided programs - in this case the hook is not called by kernel
       1 - create coredump readable by fs_uid
       2 - create coredump readable by root only
    */
    int c;
    int suid_dump_policy = 0;
    const char *filename = "/proc/sys/fs/suid_dumpable";
    FILE *f  = fopen(filename, "r");
    if (!f)
    {
        log("Can't open %s", filename);
        return suid_dump_policy;
    }

    c = fgetc(f);
    fclose(f);
    if (c != EOF)
        suid_dump_policy = c - '0';

    //log("suid dump policy is: %i", suid_dump_policy);
    return suid_dump_policy;
}

int signal_is_fatal(int signal_no, const char **name)
{
    const char *signame = NULL;
    switch (signal_no)
    {
        case SIGILL : signame = "ILL" ; break;
        case SIGFPE : signame = "FPE" ; break;
        case SIGSEGV: signame = "SEGV"; break;
        case SIGBUS : signame = "BUS" ; break; //Bus error (bad memory access)
        case SIGABRT: signame = "ABRT"; break; //usually when abort() was called
    // We have real-world reports from users who see buggy programs
    // dying with SIGTRAP, uncommented it too:
        case SIGTRAP: signame = "TRAP"; break; //Trace/breakpoint trap
    // These usually aren't caused by bugs:
      //case SIGQUIT: signame = "QUIT"; break; //Quit from keyboard
      //case SIGSYS : signame = "SYS" ; break; //Bad argument to routine (SVr4)
      //case SIGXCPU: signame = "XCPU"; break; //CPU time limit exceeded (4.2BSD)
      //case SIGXFSZ: signame = "XFSZ"; break; //File size limit exceeded (4.2BSD)
    }

    if (name != NULL)
        *name = signame;

    return signame != NULL;
}
