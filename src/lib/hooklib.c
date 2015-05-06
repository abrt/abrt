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
#include "libabrt.h"

void check_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location)
{
    struct statvfs vfs;
    if (statvfs(dump_location, &vfs) != 0)
    {
        perror_msg_and_die("statvfs('%s')", dump_location);
    }

    /* Check that at least MaxCrashReportsSize/4 MBs are free */

    /* fs_free_mb_x4 ~= vfs.f_bfree * vfs.f_bsize * 4, expressed in MBytes.
     * Need to neither overflow nor round f_bfree down too much. */
    unsigned long fs_free_mb_x4 = ((unsigned long long)vfs.f_bfree / (1024/4)) * vfs.f_bsize / 1024;
    if (fs_free_mb_x4 < setting_MaxCrashReportsSize)
    {
        error_msg_and_die("Aborting dump: only %luMiB is available on %s",
                          fs_free_mb_x4 / 4, dump_location);
    }
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
    VERB3 log("excluded_basename:'%s'", excluded_basename);

    int count = 20;
    while (--count >= 0)
    {
        /* We exclude our own dir from candidates for deletion (3rd param): */
        char *worst_basename = NULL;
        double cur_size = get_dirsize_find_largest_dir(dirname, &worst_basename, excluded_basename);
        if (cur_size <= cap_size || !worst_basename)
        {
            VERB2 log("cur_size:%.0f cap_size:%.0f, no (more) trimming", cur_size, cap_size);
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
static char* exec_vp(char **args, int redirect_stderr, unsigned exec_timeout_sec, int *status)
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
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + exec_timeout_sec;

    struct strbuf *buf_out = strbuf_new();

    ndelay_on(pipeout[0]);
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
    pid_t child = fork_execv_on_steroids(flags, args, pipeout, /*env_vec:*/ NULL, /*dir:*/ NULL, /*unused(uid)*/ 0);
    free(args[1]);

    /* Bugs in unstrip or corrupted coredumps can cause it to enter infinite loop.
     * Therefore we have a (largish) timeout, after which we kill the child.
     */
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + timeout_sec;
    struct strbuf *buf_out = strbuf_new();
    while (1)
    {
        int timeout = endtime - t;
        if (timeout < 0)
        {
            kill(child, SIGKILL);
            strbuf_append_strf(buf_out, "\nTimeout exceeded: %u seconds, killing %s\n", timeout_sec, args[0]);
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

    /* Prevent having zombie child process */
    int status;
    safe_waitpid(child, &status, 0);

    if (status != 0)
    {
        /* unstrip didnt exit with exit code 0 */
        strbuf_free(buf_out);
        return NULL;
    }

    return strbuf_free_nobuf(buf_out);
}

char *get_backtrace(const char *dump_dir_name, unsigned timeout_sec, const char *debuginfo_dirs)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    /* Let user know what's going on */
    log(_("Generating backtrace"));

    char *args[21];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";
    args[2] = (char*)"-ex";
    struct strbuf *set_debug_file_directory = strbuf_new();
    if(debuginfo_dirs == NULL)
    {
        // set non-existent debug file directory to prevent resolving
        // function names - we need offsets for core backtrace.
        strbuf_append_str(set_debug_file_directory, "set debug-file-directory /");
    }
    else
    {
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
    args[17] = (char*)"info all-registers";
    args[18] = (char*)"-ex";
    args[19] = (char*)"disassemble";
    args[20] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 1024;
    const char *thread_apply_all = "thread apply all";
    const char *full = " full";
    char *bt = NULL;
    while (1)
    {
        args[9] = xasprintf("%s backtrace %u%s", thread_apply_all, bt_depth, full);
        bt = exec_vp(args, /*redirect_stderr:*/ 1, timeout_sec, NULL);
        free(args[9]);
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
        args[19] = (char*)"disassemble $pc-20, $pc+64";

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

    free(args[3]);
    free(args[5]);
    free(args[7]);
    return bt;
}

bool dir_is_in_dump_location(const char *dir_name)
{
    unsigned len = strlen(g_settings_dump_location);

    /* The path must start with "g_settings_dump_location" */
    if (strncmp(dir_name, g_settings_dump_location, len) != 0)
    {
        VERB2 log("Bad parent directory: '%s' not in '%s'", g_settings_dump_location, dir_name);
        return false;
    }

    /* and must be a sub-directory of the g_settings_dump_location dir */
    const char *base_name = dir_name + len;
    while (*base_name && *base_name == '/')
        ++base_name;

    if (*(base_name - 1) != '/' || !str_is_correct_filename(base_name))
    {
        VERB2 log("Invalid dump directory name: '%s'", base_name);
        return false;
    }

    /* and we are sure it is a directory */
    struct stat sb;
    if (lstat(dir_name, &sb) < 0)
    {
        VERB2 perror_msg("stat('%s')", dir_name);
        return errno== ENOENT;
    }

    return S_ISDIR(sb.st_mode);
}

bool dir_has_correct_permissions(const char *dir_name)
{
    if (g_settings_privatereports)
    {
        struct stat statbuf;
        if (lstat(dir_name, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode))
        {
            error_msg("Path '%s' isn't directory", dir_name);
            return false;
        }
        /* Get ABRT's group gid */
        struct group *gr = getgrnam("abrt");
        if (!gr)
        {
            error_msg("Group 'abrt' does not exist");
            return false;
        }
        if (statbuf.st_uid != 0 || !(statbuf.st_gid == 0 || statbuf.st_gid == gr->gr_gid) || statbuf.st_mode & 07)
            return false;
    }
    return true;
}
