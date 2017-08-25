/*
    abrt-hook-ccpp.cpp - the hook for C/C++ crashing program

    Copyright (C) 2009	Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009	RedHat inc.

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
#include <fnmatch.h>
#include <sys/utsname.h>
#include "libabrt.h"
#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#else
typedef char *security_context_t;
#endif

#include <sys/resource.h>

#include <sys/types.h>

/* capabilities */
#include <sys/capability.h>

#ifdef ENABLE_DUMP_TIME_UNWIND
#include <satyr/abrt.h>
#include <satyr/utils.h>
#include <satyr/core/unwind.h>
#endif /* ENABLE_DUMP_TIME_UNWIND */

#define KERNEL_PIPE_BUFFER_SIZE 65536

static int g_user_core_flags;
static int g_need_nonrelative;

/* I want to use -Werror, but gcc-4.4 throws a curveball:
 * "warning: ignoring return value of 'ftruncate', declared with attribute warn_unused_result"
 * and (void) cast is not enough to shut it up! Oh God...
 */
#define IGNORE_RESULT(func_call) do { if (func_call) /* nothing */; } while (0)

/* Global data */
static char *user_pwd;
static DIR *proc_cwd;
static struct dump_dir *dd;

/*
 * %s - signal number
 * %c - ulimit -c value
 * %p - pid
 * %u - uid
 * %g - gid
 * %t - UNIX time of dump
 * %P - global pid
 * %I - crash thread tid
 * %e - executable filename (can contain white spaces)
 * %% - output one "%"
 */
/* Hook must be installed with exactly the same sequence of %c specifiers.
 * Last one, %h, may be omitted (we can find it out).
 */
static const char percent_specifiers[] = "%scpugtePi";
static char *core_basename = (char*) "core";

static DIR *open_cwd(pid_t pid)
{
    char buf[sizeof("/proc/%lu/cwd") + sizeof(long)*3];
    sprintf(buf, "/proc/%lu/cwd", (long)pid);

    DIR *cwd = opendir(buf);
    if (cwd == NULL)
        perror_msg("Can't open process's CWD for CompatCore");

    return cwd;
}

/* Computes a security context of new file created by the given process with
 * pid in the given directory represented by file descriptor.
 *
 * On errors returns negative number. Returns 0 if the function succeeds and
 * computes the context and returns positive number and assigns NULL to newcon
 * if the security context is not needed (SELinux disabled).
 */
static int compute_selinux_con_for_new_file(pid_t pid, int dir_fd, security_context_t *newcon)
{
#ifdef HAVE_SELINUX
    security_context_t srccon;
    security_context_t dstcon;

    const int r = is_selinux_enabled();
    if (r == 0)
    {
        *newcon = NULL;
        return 1;
    }
    else if (r == -1)
    {
        perror_msg("Couldn't get state of SELinux");
        return -1;
    }
    else if (r != 1)
        error_msg_and_die("Unexpected SELinux return value: %d", r);


    if (getpidcon_raw(pid, &srccon) < 0)
    {
        perror_msg("getpidcon_raw(%d)", pid);
        return -1;
    }

    if (fgetfilecon_raw(dir_fd, &dstcon) < 0)
    {
        perror_msg("getfilecon_raw(%s)", user_pwd);
        return -1;
    }

    if (security_compute_create_raw(srccon, dstcon, string_to_security_class("file"), newcon) < 0)
    {
        perror_msg("security_compute_create_raw(%s, %s, 'file')", srccon, dstcon);
        return -1;
    }

    return 0;
#else
    *newcon = NULL;
    return 1;
#endif
}

#ifndef HAVE_SELINUX
static int setfscreatecon_raw(security_context_t context)
{
    return -1;
}
#endif

static int open_user_core(uid_t uid, uid_t fsuid, gid_t fsgid, pid_t pid, char **percent_values)
{
    proc_cwd = open_cwd(pid);
    if (proc_cwd == NULL)
        return -1;

    /* http://article.gmane.org/gmane.comp.security.selinux/21842 */
    security_context_t newcon;
    if (compute_selinux_con_for_new_file(pid, dirfd(proc_cwd), &newcon) < 0)
    {
        log_notice("Not going to create a user core due to SELinux errors");
        return -1;
    }

    if (strcmp(core_basename, "core") == 0)
    {
        /* Mimic "core.PID" if requested */
        char buf[] = "0\n";
        int fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
        if (fd >= 0)
        {
            IGNORE_RESULT(read(fd, buf, sizeof(buf)));
            close(fd);
        }
        if (strcmp(buf, "1\n") == 0)
        {
            core_basename = xasprintf("%s.%lu", core_basename, (long)pid);
        }
    }
    else
    {
        /* Expand old core pattern, put expanded name in core_basename */
        core_basename = xstrdup(core_basename);
        unsigned idx = 0;
        while (1)
        {
            char c = core_basename[idx];
            if (!c)
                break;
            idx++;
            if (c != '%')
                continue;

            /* We just copied %, look at following char and expand %c */
            c = core_basename[idx];
            unsigned specifier_num = strchrnul(percent_specifiers, c) - percent_specifiers;
            if (percent_specifiers[specifier_num] != '\0') /* valid %c (might be %% too) */
            {
                const char *val = "%";
                if (specifier_num > 0) /* not %% */
                    val = percent_values[specifier_num - 1];
                //log_warning("c:'%c'", c);
                //log_warning("val:'%s'", val);

                /* Replace %c at core_basename[idx] by its value */
                idx--;
                char *old = core_basename;
                core_basename = xasprintf("%.*s%s%s", idx, core_basename, val, core_basename + idx + 2);
                //log_warning("pos:'%*s|'", idx, "");
                //log_warning("new:'%s'", core_basename);
                //log_warning("old:'%s'", old);
                free(old);
                idx += strlen(val);
            }
            /* else: invalid %c, % is already copied verbatim,
             * next loop iteration will copy c */
        }
    }

    if (g_need_nonrelative && core_basename[0] != '/')
    {
        error_msg("Current suid_dumpable policy prevents from saving core dumps according to relative core_pattern");
        return -1;
    }

    /* Open (create) compat core file.
     * man core:
     * There are various circumstances in which a core dump file
     * is not produced:
     *
     * [skipped obvious ones]
     * The process does not have permission to write the core file.
     * ...if a file with the same name exists and is not writable
     * or is not a regular file (e.g., it is a directory or a symbolic link).
     *
     * A file with the same name already exists, but there is more
     * than one hard link to that file.
     *
     * The file system where the core dump file would be created is full;
     * or has run out of inodes; or is mounted read-only;
     * or the user has reached their quota for the file system.
     *
     * The RLIMIT_CORE or RLIMIT_FSIZE resource limits for the process
     * are set to zero.
     * [we check RLIMIT_CORE, but how can we check RLIMIT_FSIZE?]
     *
     * The binary being executed by the process does not have
     * read permission enabled. [how we can check it here?]
     *
     * The process is executing a set-user-ID (set-group-ID) program
     * that is owned by a user (group) other than the real
     * user (group) ID of the process. [TODO?]
     * (However, see the description of the prctl(2) PR_SET_DUMPABLE operation,
     * and the description of the /proc/sys/fs/suid_dumpable file in proc(5).)
     */

    int user_core_fd = -1;
    int selinux_fail = 1;

    /*
     * These calls must be reverted as soon as possible.
     */
    xsetegid(fsgid);
    xseteuid(fsuid);

    /* Set SELinux context like kernel when creating core dump file.
     * This condition is TRUE if */
    if (/* SELinux is disabled  */ newcon == NULL
     || /* or the call succeeds */ setfscreatecon_raw(newcon) >= 0)
    {
        /* Do not O_TRUNC: if later checks fail, we do not want to have file already modified here */
        user_core_fd = openat(dirfd(proc_cwd), core_basename, O_WRONLY | O_CREAT | O_NOFOLLOW | g_user_core_flags, 0600); /* kernel makes 0600 too */

        /* Do the error check here and print the error message in order to
         * avoid interference in 'errno' usage caused by SELinux functions */
        if (user_core_fd < 0)
            perror_msg("Can't open '%s' at '%s'", core_basename, user_pwd);

        /* Fail if SELinux is enabled and the call fails */
        if (newcon != NULL && setfscreatecon_raw(NULL) < 0)
            perror_msg("setfscreatecon_raw(NULL)");
        else
            selinux_fail = 0;
    }
    else
        perror_msg("setfscreatecon_raw(%s)", newcon);

    /*
     * DON'T JUMP OVER THIS REVERT OF THE UID/GID CHANGES
     */
    xsetegid(0);
    xseteuid(0);

    if (user_core_fd < 0 || selinux_fail)
        goto user_core_fail;

    struct stat sb;
    if (fstat(user_core_fd, &sb) != 0
     || !S_ISREG(sb.st_mode)
     || sb.st_nlink != 1
     || sb.st_uid != fsuid
    ) {
        perror_msg("'%s' at '%s' is not a regular file with link count 1 owned by UID(%d)", core_basename, user_pwd, fsuid);
        goto user_core_fail;
    }
    if (ftruncate(user_core_fd, 0) != 0) {
        /* perror first, otherwise unlink may trash errno */
        perror_msg("Can't truncate '%s' at '%s' to size 0", core_basename, user_pwd);
        goto user_core_fail;
    }

    return user_core_fd;

user_core_fail:
    if (user_core_fd >= 0)
        close(user_core_fd);
    return -1;
}

static int close_user_core(int user_core_fd, off_t core_size)
{
    if (user_core_fd >= 0 && (fsync(user_core_fd) != 0 || close(user_core_fd) != 0 || core_size < 0))
    {
        perror_msg("Error writing '%s' at '%s'", core_basename, user_pwd);
        return -1;
    }
    return 0;
}

static ssize_t splice_entire_per_partes(int in_fd, int out_fd, size_t size_limit)
{
    size_t bytes = 0;
    size_t soft_limit = KERNEL_PIPE_BUFFER_SIZE;
    while (bytes < size_limit)
    {
        const size_t hard_limit = size_limit - bytes;
        if (hard_limit < soft_limit)
            soft_limit = hard_limit;

        const ssize_t copied = splice(in_fd, NULL, out_fd, NULL, soft_limit, SPLICE_F_MOVE | SPLICE_F_MORE);
        if (copied < 0)
            return copied;

        bytes += copied;

        /* Check EOF. */
        if (copied == 0)
            break;
    }

    return bytes;
}

static int create_user_core(int user_core_fd, pid_t pid, off_t ulimit_c)
{
    int err = 1;
    if (user_core_fd >= 0)
    {
        errno = 0;
        ssize_t core_size = splice_entire_per_partes(STDIN_FILENO, user_core_fd, ulimit_c);
        if (core_size < 0)
            perror_msg("Failed to create user core '%s' in '%s'", core_basename, user_pwd);

        if (close_user_core(user_core_fd, core_size) != 0 || core_size < 0)
            goto finito;

        log_notice("Saved core dump of pid %lu to '%s' at '%s' (%llu bytes)", (long)pid, core_basename, user_pwd, (long long)core_size);
    }
    err = 0;

finito:
    if (proc_cwd != NULL)
    {
        closedir(proc_cwd);
        proc_cwd = NULL;
    }

    return err;
}

static bool is_path_ignored(const GList *list, const char *path)
{
    const GList *li;
    for (li = list; li != NULL; li = g_list_next(li))
    {
        if (fnmatch((char*)li->data, path, /*flags:*/ 0) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool is_user_allowed(uid_t uid, const GList *list)
{
    const GList *li;
    for (li = list; li != NULL; li = g_list_next(li))
    {
        const char *username = (const char*)li->data;
        struct passwd *pw = getpwnam(username);
        if (pw == NULL)
        {
            log_warning("can't get uid of user '%s' (listed in 'AllowedUsers')", username);
            continue;
        }

        if(pw->pw_uid == uid)
            return true;
    }
    return false;
}

static bool is_user_in_allowed_group(uid_t uid, const GList *list)
{
    const GList *li;
    for (li = list; li != NULL; li = g_list_next(li))
    {
        const char *groupname = (const char*)li->data;
        struct group *gr = getgrnam(groupname);
        if (gr == NULL)
        {
            log_warning("can't get gid of group '%s' (listed in 'AllowedGroups')", groupname);
            continue;
        }

        if(uid_in_group(uid, gr->gr_gid))
            return true;
    }
    return false;
}

static int test_configuration(bool setting_SaveFullCore, bool setting_CreateCoreBacktrace)
{
    if (!setting_SaveFullCore && !setting_CreateCoreBacktrace)
    {
        fprintf(stderr, "Both SaveFullCore and CreateCoreBacktrace are disabled - "
                        "at least one of them is needed for useful report.\n");
        return 1;
    }

#ifndef ENABLE_DUMP_TIME_UNWIND
        fprintf(stderr, "SaveFullCore is disabled but dump time unwinding is not supported\n");
#endif /*ENABLE_DUMP_TIME_UNWIND*/

    return 0;
}

static int save_crashing_binary(pid_t pid, struct dump_dir *dd)
{
    char buf[sizeof("/proc/%lu/exe") + sizeof(long)*3];

    sprintf(buf, "/proc/%lu/exe", (long)pid);
    int src_fd_binary = open(buf, O_RDONLY); /* might fail and return -1, it's ok */
    if (src_fd_binary < 0)
    {
        log_notice("Failed to open an image of crashing binary");
        return 0;
    }

    int dst_fd = openat(dd->dd_fd, FILENAME_BINARY, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, DEFAULT_DUMP_DIR_MODE);
    if (dst_fd < 0)
    {
        log_notice("Failed to create file '"FILENAME_BINARY"' at '%s'", dd->dd_dirname);
        close(src_fd_binary);
        return -1;
    }

    IGNORE_RESULT(fchown(dst_fd, dd->dd_uid, dd->dd_gid));

    off_t sz = copyfd_eof(src_fd_binary, dst_fd, COPYFD_SPARSE);
    close(src_fd_binary);

    return fsync(dst_fd) != 0 || close(dst_fd) != 0 || sz < 0;
}

static void error_msg_process_crash(const char *pid_str, const char *process_str,
        long unsigned uid, int signal_no, const char *signame, const char *message, ...)
{
    va_list p;
    va_start(p, message);
    char *message_full = xvasprintf(message, p);
    va_end(p);

    char *process_name = (process_str) ?  xasprintf(" (%s)", process_str) : xstrdup("");

    if (signame)
        error_msg("Process %s%s of user %lu killed by SIG%s - %s", pid_str,
                        process_name, uid, signame, message_full);
    else
        error_msg("Process %s%s of user %lu killed by signal %d - %s", pid_str,
                        process_name, uid, signal_no, message_full);

    free(process_name);
    free(message_full);

    return;
}

static void error_msg_ignore_crash(const char *pid_str, const char *process_str,
        long unsigned uid, int signal_no, const char *signame, const char *message, ...)
{
    va_list p;
    va_start(p, message);
    char *message_full = xvasprintf(message, p);
    va_end(p);

    error_msg_process_crash(pid_str, process_str, uid, signal_no, signame, "ignoring (%s)", message_full);

    free(message_full);
    return;
}

static void dump_abrt_process(pid_t pid, const char *executable)
{
    /* If abrtd/abrt-foo crashes, we don't want to create a _directory_,
     * since that can make new copy of abrtd to process it,
     * and maybe crash again...
     * Unlike dirs, mere files are ignored by abrtd.
     */
    const char *basename = strrchr(executable, '/') + 1;
    char *path = xasprintf("%s/%s-coredump", g_settings_dump_location, basename);
    unlink(path);
    int abrt_core_fd = xopen3(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    off_t core_size = splice_entire_per_partes(STDIN_FILENO, abrt_core_fd, SIZE_MAX);
    if (core_size < 0 || fsync(abrt_core_fd) != 0 || close(abrt_core_fd) < 0)
    {
        unlink(path);
        /* copyfd_eof logs the error including errno string,
         * but it does not log file name */
        error_msg_and_die("Error saving '%s'", path);
    }
    log_notice("Saved core dump of pid %lu (%s) to %s (%llu bytes)", (long)pid, basename, path, (long long)core_size);
    free(path);
}

static ssize_t splice_full(int in_fd, int out_fd, size_t size)
{
    ssize_t total = 0;
    while (size != 0)
    {
        const ssize_t b = splice(in_fd, NULL, out_fd, NULL, size, 0);
        if (b < 0)
            return b;

        if (b == 0)
            break;

        total += b;
        size -= b;
    }

    return total;
}

static size_t xsplice_full(int in_fd, int out_fd, size_t size)
{
    const ssize_t r = splice_full(in_fd, out_fd, size);
    if (r < 0)
        perror_msg_and_die("Failed to write core dump to file");
    return (size_t)r;
}

static void pipe_close(int *pfds)
{
    close(pfds[0]);
    close(pfds[1]);
    pfds[0] = pfds[1] = -1;
}

enum dump_core_files_ret_flags {
    DUMP_ABRT_CORE_FAILED  = 0x0001,
    DUMP_USER_CORE_FAILED  = 0x0100,
};

/* Optimized creation of two core files - ABRT and CWD
 *
 * The simplest optimization is to avoid the need to copy data to user space.
 * In that case we cannot read data once and write them twice as we do with
 * read/write approach because there is no syscall forwarding data from a
 * single source fd to several destination fds (one might claim that there is
 * tee() function but such a solution is suboptimal from our perspective).
 *
 * So the function first create ABRT core file and then creates user core file.
 * If ABRT limit made the ABRT core to be smaller than allowed user core size,
 * then the function reads more data from STDIN and appends them to the user
 * core file.
 *
 * We must not read from the user core fd because that operation might be
 * refused by OS.
 */
static int dump_two_core_files(int abrt_core_fd, size_t *abrt_limit, int user_core_fd, size_t *user_limit)
{
   /* tee() does not move the in_fd, thus you need to call splice to be
    * get next chunk of data loaded into the in_fd buffer.
    * So, calling tee() without splice() would be looping on the same
    * data. Hence, we must ensure that after tee() we call splice() and
    * that would be problematic if tee core limit is greater than splice
    * core limit. Therefore, we swap the out fds based on their limits.
    */
    int    spliced_fd          = *abrt_limit > *user_limit ? abrt_core_fd    : user_core_fd;
    size_t spliced_core_limit  = *abrt_limit > *user_limit ? *abrt_limit     : *user_limit;
    int    teed_fd             = *abrt_limit > *user_limit ? user_core_fd    : abrt_core_fd;
    size_t teed_core_limit     = *abrt_limit > *user_limit ? *user_limit     : *abrt_limit;

    size_t *spliced_core_size  = *abrt_limit > *user_limit ? abrt_limit : user_limit;
    size_t *teed_core_size     = *abrt_limit > *user_limit ? user_limit : abrt_limit;

    *spliced_core_size = *teed_core_size = 0;

    int cp[2] = { -1, -1 };
    if (pipe(cp) < 0)
    {
        perror_msg("Failed to create temporary pipe for core file");
        cp[0] = cp[1] = -1;
    }

    /* tee() can copy duplicate up to size of the pipe buffer bytes.
     * It should not be problem to ask for more (in that case, tee would simply
     * duplicate up to the limit bytes) but I would rather not to exceed
     * the pipe buffer limit.
     */
    int copy_buffer_size = fcntl(STDIN_FILENO, F_GETPIPE_SZ);
    if (copy_buffer_size < 0)
        copy_buffer_size = KERNEL_PIPE_BUFFER_SIZE;

    ssize_t to_write = copy_buffer_size;
    for (;;)
    {
        if (cp[1] >= 0)
        {
            to_write = tee(STDIN_FILENO, cp[1], copy_buffer_size, 0);

            /* Check EOF. */
            if (to_write == 0)
                break;

            if (to_write < 0)
            {
                perror_msg("Cannot duplicate stdin buffer for core file");
                pipe_close(cp);
                to_write = copy_buffer_size;
            }
        }

        size_t to_splice = to_write;
        if (*spliced_core_size + to_splice > spliced_core_limit)
            to_splice = spliced_core_limit - *spliced_core_size;

        const size_t spliced = xsplice_full(STDIN_FILENO, spliced_fd, to_splice);
        *spliced_core_size += spliced;

        if (cp[0] >= 0)
        {
            size_t to_tee = to_write;
            if (*teed_core_size + to_tee > teed_core_limit)
                to_tee = teed_core_limit - *teed_core_size;

            const ssize_t teed = splice_full(cp[0], teed_fd, to_tee);
            if (teed < 0)
            {
                perror_msg("Cannot splice teed data to core file");
                pipe_close(cp);
                to_write = copy_buffer_size;
            }
            else
                *teed_core_size += teed;

            if (*teed_core_size >= teed_core_limit)
            {
                pipe_close(cp);
                to_write = copy_buffer_size;
            }
        }

        /* Check EOF. */
        if (spliced == 0 || *spliced_core_size >= spliced_core_limit)
            break;
    }

    int r = 0;
    if (cp[0] < 0)
    {
        if (abrt_limit < user_limit)
            r |= DUMP_ABRT_CORE_FAILED;
        else
            r |= DUMP_USER_CORE_FAILED;
    }
    else
        pipe_close(cp);

    return r;
}

enum create_core_backtrace_status
{
    CB_DISABLED     = 0x1,
    CB_STDIN_CLOSED = 0x2,
    CB_SUCCESSFUL   = 0x4,
};

static enum create_core_backtrace_status
create_core_backtrace(struct dump_dir *dd, uid_t uid, uid_t fsuid, gid_t gid,
                      gid_t fsgid, pid_t tid, const char *executable, int signal_no)
{
#ifndef ENABLE_DUMP_TIME_UNWIND
    return CB_DISABLED;
#else  /*ENABLE_DUMP_TIME_UNWIND*/
    int retval = 0;
    pid_t pid = fork();
    if (pid < 0)
    {
        perror_msg("fork");
        goto no_core_backtrace_generated_failure;
    }

    if (pid == 0)
    {
        if (g_verbose > 1)
            sr_debug_parser = true;

        const int corebtfd = dd_open_item(dd, FILENAME_CORE_BACKTRACE, O_RDWR);
        if (corebtfd < 0)
            perror_msg_and_die("Cannot open %s", FILENAME_CORE_BACKTRACE);

        char *error_message = NULL;
        struct sr_core_stracetrace_unwind_state *state = NULL;
        state = sr_abrt_get_core_stacktrace_from_core_hook_prepare(tid, &error_message);

        if (error_message)
            perror_msg_and_die("Can't prepare for core backtrace generation: %s", error_message);

        const gid_t g = gid == 0 ? fsgid : gid;
        if (setresgid(g, g, g) == -1)
            perror_msg_and_die("Can't change process group id of '%d' user", gid);

        const uid_t u = uid == 0 ? fsuid : uid;
        if (setresuid(u, u, u) == -1)
            perror_msg_and_die("Can't change process user id of '%d' user", uid);

        log_debug("Running core_backtrace under %d:%d", u, g);

        /* Get capability state of the calling process  */
        cap_t caps = cap_get_proc();
        if (!caps)
            perror_msg_and_die("Can't get capability state of process PID: %d", getpid());

        /* Array must be filled with CAP_* constants */
        cap_value_t cap_list[CAP_LAST_CAP+1];
        for (cap_value_t cap = CAP_CHOWN; cap <= CAP_LAST_CAP; cap++)
            cap_list[cap] = cap;

        if (cap_set_flag(caps, CAP_PERMITTED, CAP_LAST_CAP, cap_list, CAP_CLEAR) == -1)
            perror_msg_and_die("Failed to clear all capabilities in permitted set");

        if (cap_set_flag(caps, CAP_EFFECTIVE, CAP_LAST_CAP, cap_list, CAP_CLEAR) == -1)
            perror_msg_and_die("Failed to clear all capabilities in effective set");

        if (cap_set_flag(caps, CAP_INHERITABLE, CAP_LAST_CAP, cap_list, CAP_CLEAR) == -1)
            perror_msg_and_die("Failed to clear all capabilities in inherited set");

        if (cap_set_proc(caps) == -1)
            perror_msg_and_die("Failed to assign cleared capabilities to process");

        if (cap_free(caps) == -1)
            perror_msg_and_die("Error releasing capability state resource! PID: %d", getpid());

        char *json = sr_abrt_get_core_stacktrace_from_core_hook_generate(tid, executable,
                                                                         signal_no, state,
                                                                         &error_message);
        state = NULL;
        if (!json)
            error_msg_and_die("Can't generate core backtrace: %s", error_message);

        full_write_str(corebtfd, json);
        free(json);
        close(corebtfd);

        exit(0);
    }

    /* Both processes must close its stdin! */
    close(STDIN_FILENO);
    retval |= CB_STDIN_CLOSED;

    int status = 0;
    if (safe_waitpid(pid, &status, 0) >= 0)
    {
        if (WIFSIGNALED(status))
        {
            log_warning("Core backtrace generator signaled with %d", WTERMSIG(status));
            goto core_backtrace_failed;
        }
        if (!WIFEXITED(status))
        {
            log_warning("Core backtrace generator did not properly exit");
            goto core_backtrace_failed;
        }
        const int r = WEXITSTATUS(status);
        if (r != 0)
        {
            log_warning("Core backtrace generator exited with error %d", r);
            goto core_backtrace_failed;
        }

        log_debug("Core backtrace generator finished successfully");
        retval |= CB_SUCCESSFUL;
    }
    else
    {
        perror_msg("waitpid");
        goto core_backtrace_failed;
    }

    return retval;

core_backtrace_failed:
    {
        struct stat st;
        const int r = dd_item_stat(dd, FILENAME_CORE_BACKTRACE, &st);
        /* Either stat failed (it doesn't matter if the item does not exist) or the
         * item has 0 Bytes - there is no need to retain the item in neither case.
         */
        if (r != 0 || st.st_size == 0)
            dd_delete_item(dd, FILENAME_CORE_BACKTRACE);
    }
no_core_backtrace_generated_failure:
    return retval;
#endif /*ENABLE_DUMP_TIME_UNWIND*/
}

int main(int argc, char** argv)
{
    /* Kernel starts us with all fd's closed.
     * But it's dangerous:
     * fprintf(stderr) can dump messages into random fds, etc.
     * Ensure that if any of fd 0,1,2 is closed, we open it to /dev/null.
     */
    int fd = xopen("/dev/null", O_RDWR);
    while (fd < 2)
        fd = xdup(fd);
    if (fd > 2)
        close(fd);

    int err = 1;
    logmode = LOGMODE_JOURNAL;

    /* Parse abrt.conf */
    load_abrt_conf();

    /* core_pattern processes have RLIMIT_CORE set to 1 by default.
     * If kernel sees RLIMIT_CORE == 1 and pipe is in core_pattern, dumping
     * of core file is aborted (do_coredump() in kernel/fs/coredump.c)
     */
    if (g_settings_debug_level >= 100)
        setrlimit(RLIMIT_CORE, &((struct rlimit){ RLIM_INFINITY, RLIM_INFINITY}));
    if (g_settings_debug_level >= 200)
        set_xfunc_diemode(DIEMODE_ABORT);

    /* ... and plugins/CCpp.conf */
    bool setting_MakeCompatCore;
    bool setting_SaveBinaryImage;
    bool setting_SaveFullCore;
    bool setting_CreateCoreBacktrace;
    bool setting_SaveContainerizedPackageData;
    bool setting_StandaloneHook;
    unsigned int setting_MaxCoreFileSize = g_settings_nMaxCrashReportsSize;

    GList *setting_ignored_paths = NULL;
    GList *setting_allowed_users = NULL;
    GList *setting_allowed_groups = NULL;
    {
        map_string_t *settings = new_map_string();
        load_abrt_plugin_conf_file("CCpp.conf", settings);
        const char *value;
        value = get_map_string_item_or_NULL(settings, "MakeCompatCore");
        setting_MakeCompatCore = value && string_to_bool(value);
        value = get_map_string_item_or_NULL(settings, "SaveBinaryImage");
        setting_SaveBinaryImage = value && string_to_bool(value);
        value = get_map_string_item_or_NULL(settings, "SaveFullCore");
        setting_SaveFullCore = value ? string_to_bool(value) : true;
        value = get_map_string_item_or_NULL(settings, "CreateCoreBacktrace");
        setting_CreateCoreBacktrace = value ? string_to_bool(value) : true;
        value = get_map_string_item_or_NULL(settings, "IgnoredPaths");
        if (value)
            setting_ignored_paths = parse_list(value);

        value = get_map_string_item_or_NULL(settings, "AllowedUsers");
        if (value)
            setting_allowed_users = parse_list(value);
        value = get_map_string_item_or_NULL(settings, "AllowedGroups");
        if (value)
            setting_allowed_groups = parse_list(value);

        value = get_map_string_item_or_NULL(settings, "MaxCoreFileSize");
        if (value && !try_get_map_string_item_as_uint(settings, "MaxCoreFileSize", &setting_MaxCoreFileSize))
            log_warning("The MaxCoreFileSize option in the CCpp.conf file holds an invalid value");

        value = get_map_string_item_or_NULL(settings, "SaveContainerizedPackageData");
        setting_SaveContainerizedPackageData = value && string_to_bool(value);

        /* Do not call abrt-action-save-package-data with process's root, if ExploreChroots is disabled. */
        if (!g_settings_explorechroots)
        {
            if (setting_SaveContainerizedPackageData)
                log_warning("Ignoring SaveContainerizedPackageData because ExploreChroots is disabled");
            setting_SaveContainerizedPackageData = false;
        }

        value = get_map_string_item_or_NULL(settings, "StandaloneHook");
        setting_StandaloneHook = value && string_to_bool(value);
        value = get_map_string_item_or_NULL(settings, "VerboseLog");
        if (value)
            g_verbose = xatoi_positive(value);
        free_map_string(settings);
    }

    if (argc == 2 && !strcmp(argv[1], "--test-config"))
        return test_configuration(setting_SaveFullCore, setting_CreateCoreBacktrace);

    if (argc < 8)
    {
        /* percent specifier:         %s   %c              %p  %u  %g  %t   %P         %T        */
        /* argv:                  [0] [1]  [2]             [3] [4] [5] [6]  [7]        [8]       */
        error_msg_and_die("Usage: %s SIGNO CORE_SIZE_LIMIT PID UID GID TIME GLOBAL_PID GLOBAL_TID", argv[0]);
    }

    /* Not needed on 2.6.30.
     * At least 2.6.18 has a bug where
     * argv[1] = "SIGNO CORE_SIZE_LIMIT PID ..."
     * argv[2] = "CORE_SIZE_LIMIT PID ..."
     * and so on. Fixing it:
     */
    if (strchr(argv[1], ' '))
    {
        int i;
        for (i = 1; argv[i]; i++)
        {
            strchrnul(argv[i], ' ')[0] = '\0';
        }
    }

    const char *pid_str = argv[3];
    /* xatoi_positive() handles errors */
    uid_t uid = xatoi_positive(argv[4]);
    gid_t gid = xatoi_positive(argv[5]);

    const char* signal_str = argv[1];
    int signal_no = xatoi_positive(signal_str);
    const char *signame = NULL;
    bool signal_is_fatal_bool = signal_is_fatal(signal_no, &signame);

    errno = 0;
    off_t ulimit_c = strtoull(argv[2], NULL, 10);
    if (errno)
    {
        error_msg_ignore_crash(pid_str, NULL, (long unsigned)uid, signal_no,
                signame, "limit '%s' is bogus", argv[2]);
        xfunc_die();
    }

    if (ulimit_c < 0) /* unlimited? */
    {
        /* set to max possible >0 value */
        ulimit_c = ~((off_t)1 << (sizeof(off_t)*8-1));
    }
    const char *global_pid_str = argv[7];
    pid_t pid = xatoi_positive(argv[7]);
    const int pid_proc_fd = open_proc_pid_dir(pid);

    user_pwd = get_cwd_at(pid_proc_fd); /* may be NULL on error */
    log_notice("user_pwd:'%s'", user_pwd);

    {
        char *s = xmalloc_fopen_fgetline_fclose(VAR_RUN"/abrt/saved_core_pattern");
        /* If we have a saved pattern and it's not a "|PROG ARGS" thing... */
        if (s && s[0] != '|')
            core_basename = s;
        else
            free(s);
    }

    char path[PATH_MAX];

    sprintf(path, "/proc/%lu/status", (long)pid);
    char *proc_pid_status = xmalloc_xopen_read_close(path, /*maxsz:*/ NULL);

    uid_t fsuid = uid;
    /* int because get_fsuid() returns negative values in case of error */
    int tmp_fsuid = get_fsuid(proc_pid_status);
    if (tmp_fsuid < 0)
    {
        error_msg_ignore_crash(pid_str, NULL, (long unsigned)uid, signal_no,
                signame, "parsing error");
        xfunc_die();
    }

    const int fsgid = get_fsgid(proc_pid_status);
    if (fsgid < 0)
    {
        error_msg_ignore_crash(pid_str, NULL, (long unsigned)uid, signal_no,
                signame, "parsing error");
        xfunc_die();
    }

    int suid_policy = dump_suid_policy();
    if ((uid_t)tmp_fsuid != uid)
    {
        /* use root for suided apps unless it's explicitly set to UNSAFE */
        fsuid = 0;
        if (suid_policy == DUMP_SUID_UNSAFE)
            fsuid = (uid_t)tmp_fsuid;
        else
        {
            g_user_core_flags = O_EXCL;
            g_need_nonrelative = 1;
        }
    }

    snprintf(path, sizeof(path), "%s/last-ccpp", g_settings_dump_location);

    /* Open a fd to compat coredump, if requested and is possible */
    int user_core_fd = -1;
    if (setting_MakeCompatCore && ulimit_c != 0)
        /* note: checks "user_pwd == NULL" inside; updates core_basename */
        user_core_fd = open_user_core(uid, fsuid, fsgid, pid, &argv[1]);

    char *executable = get_executable_at(pid_proc_fd);
    if (executable == NULL)
    {
        /* readlink on /proc/$PID/exe failed, don't create abrt dump dir */
        error_msg_ignore_crash(pid_str, NULL, (long unsigned)uid, signal_no,
                signame, "Can't read /proc/%lu/exe link", (long)pid);
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    const char *last_slash = strrchr(executable, '/');
    /* if the last_slash was found, skip it */
    if (last_slash) ++last_slash;

    /* ignoring crashes */
    if (executable && is_path_ignored(setting_ignored_paths, executable))
    {
        error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "listed in 'IgnoredPaths'");

        return 0;
    }
    /* do not dump abrt-hook-ccpp crashes */
    if (executable && strstr(executable, "/abrt-hook-ccpp"))
    {
        if (g_settings_debug_level >= 100)
        {
            dump_abrt_process(pid, executable);
        }
        else
        {   /* This can happen only if there is a bug in kernel, otherwise,
             * kernel actively prevents recursion of crashes of core_pattern
             * unless core_pattern sets RLIMIT_CORE != 1.
             * (do_coredump() in kernel/fs/coredump.c)
             */
            error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid,
                                   signal_no, signame, "avoid recursion");
        }

        exit(0);
    }
    /* Check /var/tmp/abrt/last-ccpp marker, do not dump repeated crashes
     * if they happen too often. Else, write new marker value.
     */
    if (check_recent_crash_file(path, executable))
    {
        error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "repeated crash");

        /* It is a repeating crash */
        return create_user_core(user_core_fd, pid, ulimit_c);
    }
    const bool abrt_crash = (last_slash && (strncmp(last_slash, "abrt", 4) == 0));
    if (abrt_crash && g_settings_debug_level == 0)
    {
        error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "'DebugLevel' == 0");

        goto cleanup_and_exit;
    }
    /* unsupported signal */
    if (!signal_is_fatal_bool)
    {
        error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "unsupported signal");

        return create_user_core(user_core_fd, pid, ulimit_c); // not a signal we care about

    }
    const int abrtd_running = daemon_is_ok();
    if (!setting_StandaloneHook && !abrtd_running)
    {
        error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "abrtd is not running");

        /* not an error, exit with exit code 0 */
        log_warning("If abrtd crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    /* dumping core for user, if allowed */
    if (setting_allowed_users || setting_allowed_groups)
    {
        if (setting_allowed_users && is_user_allowed(uid, setting_allowed_users))
            log_debug("User %lu is listed in 'AllowedUsers'", (long unsigned)uid);
        else if (setting_allowed_groups && is_user_in_allowed_group(uid, setting_allowed_groups))
            log_debug("User %lu is member of group listed in 'AllowedGroups'", (long unsigned)uid);
        else
        {
            error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                signame, "not allowed in 'AllowedUsers' nor 'AllowedGroups'");

            xfunc_die();
        }
    }

    /* low free space */
    if (g_settings_nMaxCrashReportsSize > 0)
    {
        /* If free space is less than 1/4 of MaxCrashReportsSize... */
        if (low_free_space(g_settings_nMaxCrashReportsSize, g_settings_dump_location))
        {
            error_msg_ignore_crash(pid_str, last_slash, (long unsigned)uid, signal_no,
                                    signame, "low free space");
            return create_user_core(user_core_fd, pid, ulimit_c);
        }
    }

    // processing crash - inform user about it
    error_msg_process_crash(pid_str, last_slash, (long unsigned)uid,
                signal_no, signame, "dumping core");

    pid_t tid = -1;
    const char *tid_str = argv[8];
    if (tid_str)
    {
        tid = xatoi_positive(tid_str);
    }

    if (setting_StandaloneHook)
        ensure_writable_dir(g_settings_dump_location, DEFAULT_DUMP_LOCATION_MODE, "abrt");

    if (abrt_crash)
    {
        dump_abrt_process(pid, executable);
        err = 0;
        goto cleanup_and_exit;
    }

    unsigned path_len = snprintf(path, sizeof(path), "%s/ccpp-%s-%lu.new",
            g_settings_dump_location, iso_date_string(NULL), (long)pid);
    if (path_len >= (sizeof(path) - sizeof("/"FILENAME_COREDUMP)))
    {
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

    /* If you don't want to have fs owner as root then:
     *
     * - use fsuid instead of uid for fs owner, so we don't expose any
     *   sensitive information of suided app in /var/(tmp|spool)/abrt
     *
     * - use dd_create_skeleton() and dd_reset_ownership(), when you finish
     *   creating the new dump directory, to prevent the real owner to write to
     *   the directory until the hook is done (avoid race conditions and defend
     *   hard and symbolic link attacs)
     */
    dd = dd_create(path, /*fs owner*/0, DEFAULT_DUMP_DIR_MODE);
    if (dd)
    {
        char source_filename[sizeof("/proc/%lu/somewhat_long_name") + sizeof(long)*3];
        int source_base_ofs = sprintf(source_filename, "/proc/%lu/root", (long)pid);
        source_base_ofs -= strlen("root");

        /* What's wrong on using /proc/[pid]/root every time ?*/
        /* It creates os_info_in_root_dir for all crashes. */
        char *rootdir = process_has_own_root_at(pid_proc_fd) ? get_rootdir_at(pid_proc_fd) : NULL;

        /* Reading data from an arbitrary root directory is not secure. */
        if (g_settings_explorechroots)
        {
            /* Yes, test 'rootdir' but use 'source_filename' because 'rootdir' can
             * be '/' for a process with own namespace. 'source_filename' is /proc/[pid]/root. */
            dd_create_basic_files(dd, fsuid, (rootdir != NULL) ? source_filename : NULL);
        }
        else
        {
            dd_create_basic_files(dd, fsuid, NULL);
        }

        // Disabled for now: /proc/PID/smaps tends to be BIG,
        // and not much more informative than /proc/PID/maps:
        // dd_copy_file_at(dd, FILENAME_SMAPS, pid_proc_fd, "smaps");

        dd_copy_file_at(dd, FILENAME_MAPS, pid_proc_fd, "maps");
        dd_copy_file_at(dd, FILENAME_LIMITS, pid_proc_fd, "limits");
        dd_copy_file_at(dd, FILENAME_CGROUP, pid_proc_fd, "cgroup");
        dd_copy_file_at(dd, FILENAME_MOUNTINFO, pid_proc_fd, "mountinfo");

        FILE *open_fds = dd_open_item_file(dd, FILENAME_OPEN_FDS, O_RDWR);
        if (open_fds != NULL)
        {
            if (dump_fd_info_at(pid_proc_fd, open_fds) < 0)
                dd_delete_item(dd, FILENAME_OPEN_FDS);
            fclose(open_fds);
        }

        const int init_proc_dir_fd = open_proc_pid_dir(1);
        FILE *namespaces = dd_open_item_file(dd, FILENAME_NAMESPACES, O_RDWR);
        if (namespaces != NULL && init_proc_dir_fd >= 0)
        {
            if (dump_namespace_diff_at(init_proc_dir_fd, pid_proc_fd, namespaces) < 0)
                dd_delete_item(dd, FILENAME_NAMESPACES);
        }
        if (init_proc_dir_fd >= 0)
            close(init_proc_dir_fd);
        if (namespaces != NULL)
            fclose(namespaces);

        /* There's no need to compare mount namespaces and search for '/' in
         * mountifo.  Comparison of inodes of '/proc/[pid]/root' and '/' works
         * fine. If those inodes do not equal each other, we have to verify
         * that '/proc/[pid]/root' is not a symlink to a chroot.
         */
        const int containerized = (rootdir != NULL && strcmp(rootdir, "/") == 0);
        if (containerized)
        {
            log_debug("Process %d is considered to be containerized", pid);
            pid_t container_pid;
            if (get_pid_of_container_at(pid_proc_fd, &container_pid) == 0)
            {
                char *container_cmdline = get_cmdline(container_pid);
                dd_save_text(dd, FILENAME_CONTAINER_CMDLINE, container_cmdline);
                free(container_cmdline);
            }
        }

        dd_save_text(dd, FILENAME_ANALYZER, "abrt-ccpp");
        dd_save_text(dd, FILENAME_TYPE, "CCpp");
        dd_save_text(dd, FILENAME_EXECUTABLE, executable);
        dd_save_text(dd, FILENAME_PID, pid_str);
        dd_save_text(dd, FILENAME_GLOBAL_PID, global_pid_str);
        dd_save_text(dd, FILENAME_PROC_PID_STATUS, proc_pid_status);
        if (user_pwd)
            dd_save_text(dd, FILENAME_PWD, user_pwd);
        if (tid_str)
            dd_save_text(dd, FILENAME_TID, tid_str);

        if (rootdir)
        {
            if (strcmp(rootdir, "/") != 0)
                dd_save_text(dd, FILENAME_ROOTDIR, rootdir);
        }
        free(rootdir);

        char *reason = xasprintf("%s killed by SIG%s",
                                 last_slash, signame ? signame : signal_str);
        dd_save_text(dd, FILENAME_REASON, reason);
        free(reason);

        char *cmdline = get_cmdline_at(pid_proc_fd);
        dd_save_text(dd, FILENAME_CMDLINE, cmdline ? : "");
        free(cmdline);

        char *environ = get_environ_at(pid_proc_fd);
        dd_save_text(dd, FILENAME_ENVIRON, environ ? : "");
        free(environ);

        char *fips_enabled = xmalloc_fopen_fgetline_fclose("/proc/sys/crypto/fips_enabled");
        if (fips_enabled)
        {
            if (strcmp(fips_enabled, "0") != 0)
                dd_save_text(dd, "fips_enabled", fips_enabled);
            free(fips_enabled);
        }

        dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);

        /* In case of errors, treat the process as if it has locked memory */
        long unsigned lck_bytes = ULONG_MAX;
        const char *vmlck = strstr(proc_pid_status, "VmLck:");
        if (vmlck == NULL)
            error_msg("/proc/%s/status does not contain 'VmLck:' line", pid_str);
        else if (1 != sscanf(vmlck + 6, "%lu kB\n", &lck_bytes))
            error_msg("Failed to parse 'VmLck:' line in /proc/%s/status", pid_str);

        if (lck_bytes)
        {
            log_notice("Process %s of user %lu has locked memory",
                        pid_str, (long unsigned)uid);

            dd_mark_as_notreportable(dd, "The process had locked memory "
                    "which usually indicates efforts to protect sensitive "
                    "data (passwords) from being written to disk.\n"
                    "In order to avoid sensitive information leakages, "
                    "ABRT will not allow you to report this problem to "
                    "bug tracking tools");
        }

        if (setting_SaveBinaryImage)
        {
            if (save_crashing_binary(pid, dd))
            {
                error_msg("Error saving '%s'", path);

                goto cleanup_and_exit;
            }
        }

        size_t core_size = 0;
        if (setting_SaveFullCore)
        {
            int abrt_core_fd = dd_open_item(dd, FILENAME_COREDUMP, O_RDWR);
            if (abrt_core_fd < 0)
            {   /* Avoid the need to deal with two destinations. */
                perror_msg("Failed to create ABRT core file in '%s'", dd->dd_dirname);
                create_user_core(user_core_fd, pid, ulimit_c);
            }
            else
            {
                size_t abrt_limit = 0;
                if (   (g_settings_nMaxCrashReportsSize != 0 && setting_MaxCoreFileSize == 0)
                    || (g_settings_nMaxCrashReportsSize != 0 && g_settings_nMaxCrashReportsSize < setting_MaxCoreFileSize))
                    abrt_limit = g_settings_nMaxCrashReportsSize;
                else
                    abrt_limit = setting_MaxCoreFileSize;

                if (abrt_limit != 0)
                {
                    const size_t abrt_limit_bytes = 1024 * 1024 * abrt_limit;
                    /* Overflow protection. */
                    if (abrt_limit_bytes > abrt_limit)
                        abrt_limit = abrt_limit_bytes;
                    else
                    {
                        error_msg("ABRT core file size limit (MaxCrashReportsSize|MaxCoreFileSize) does not fit into runtime type. Using maximal possible size.");
                        abrt_limit = SIZE_MAX;
                    }
                }
                else
                    abrt_limit = SIZE_MAX;

                if (user_core_fd < 0)
                {
                    const ssize_t r = splice_entire_per_partes(STDIN_FILENO, abrt_core_fd, abrt_limit);
                    if (r < 0)
                        perror_msg("Failed to write ABRT core file");
                    else
                        core_size = r;
                }
                else
                {
                    size_t user_limit = ulimit_c;
                    const int r = dump_two_core_files(abrt_core_fd, &abrt_limit, user_core_fd, &user_limit);

                    close_user_core(user_core_fd, (r & DUMP_USER_CORE_FAILED) ? -1 : user_limit);

                    if (!(r & DUMP_ABRT_CORE_FAILED))
                        core_size = abrt_limit;
                }

                if (fsync(abrt_core_fd) != 0 || close(abrt_core_fd) != 0)
                    perror_msg("Failed to close ABRT core file");
            }
        }
        else
        {
            /* User core is created even if WriteFullCore is off. */
            create_user_core(user_core_fd, pid, ulimit_c);
        }

        /* User core is either written or closed */
        user_core_fd = -1;

        /*
         * ! No other errors should cause removal of the user core !
         */

/* Because of #1211835 and #1126850 */
#if 0
        /* Save JVM crash log if it exists. (JVM's coredump per se
         * is nearly useless for JVM developers)
         */
        {
            char *java_log = xasprintf("/tmp/jvm-%lu/hs_error.log", (long)pid);
            int src_fd = open(java_log, O_RDONLY);
            free(java_log);

            /* If we couldn't open the error log in /tmp directory we can try to
             * read the log from the current directory. It may produce AVC, it
             * may produce some error log but all these are expected.
             */
            if (src_fd < 0)
            {
                java_log = xasprintf("%s/hs_err_pid%lu.log", user_pwd, (long)pid);
                src_fd = open(java_log, O_RDONLY);
                free(java_log);
            }

            if (src_fd >= 0)
            {
                strcpy(path + path_len, "/hs_err.log");
                int dst_fd = create_or_die(path, user_core_fd);
                off_t sz = copyfd_eof(src_fd, dst_fd, COPYFD_SPARSE);
                if (close(dst_fd) != 0 || sz < 0)
                {
                    error_msg("Error saving '%s'", path);

                    goto cleanup_and_exit;
                }
                close(src_fd);
            }
        }
#endif

        if (abrtd_running && setting_SaveContainerizedPackageData && containerized)
        {   /* Do we really need to run rpm from core_pattern hook? */
            sprintf(source_filename, "/proc/%lu/root", (long)pid);

            const char *cmd_args[6];
            cmd_args[0] = BIN_DIR"/abrt-action-save-package-data";
            cmd_args[1] = "-d";
            cmd_args[2] = path;
            cmd_args[3] = "-r";
            cmd_args[4] = source_filename;
            cmd_args[5] = NULL;

            pid_t pid = fork_execv_on_steroids(0, (char **)cmd_args, NULL, NULL, path, 0);
            int stat;
            safe_waitpid(pid, &stat, 0);
        }

        enum create_core_backtrace_status cbr = 0;
        /* Perform crash-time unwind of the guilty thread. */
        if (tid > 0 && setting_CreateCoreBacktrace)
        {
            log_debug("Creating core_backtrace\n");
            cbr = create_core_backtrace(dd, uid, fsuid, gid, fsgid, tid, executable, signal_no);
            if (cbr & CB_DISABLED)
                log_warning("CreateCoreBacktrace is enabled but dump time unwinding is not supported");
        }

        /* Make sure we closed STDIN_FILENO to let kernel to wipe out the process. */
        if (!(cbr & CB_STDIN_CLOSED))
            close(STDIN_FILENO);

        /* We close dumpdir before we start catering for crash storm case.
         * Otherwise, delete_dump_dir's from other concurrent
         * CCpp's won't be able to delete our dump (their delete_dump_dir
         * will wait for us), and we won't be able to delete their dumps.
         * Classic deadlock.
         */
        dd_close(dd);
        dd = NULL;

        path[path_len] = '\0'; /* path now contains only directory name */

        char *newpath = xstrndup(path, path_len - (sizeof(".new")-1));
        if (rename(path, newpath) == 0)
            strcpy(path, newpath);
        free(newpath);

        if (core_size > 0)
            log_notice("Saved core dump of pid %lu (%s) to %s (%zu bytes)",
                       (long)pid, executable, path, core_size);

        if (abrtd_running)
            notify_new_path(path);

        /* rhbz#539551: "abrt going crazy when crashing process is respawned" */
        if (g_settings_nMaxCrashReportsSize > 0)
        {
            /* x1.25 and round up to 64m: go a bit up, so that usual in-daemon trimming
             * kicks in first, and we don't "fight" with it:
             */
            unsigned maxsize = g_settings_nMaxCrashReportsSize + g_settings_nMaxCrashReportsSize / 4;
            maxsize |= 63;
            trim_problem_dirs(g_settings_dump_location, maxsize * (double)(1024*1024), path);
        }

        err = 0;
    }
    else
    {
        /* We didn't create abrt dump, but may need to create compat coredump */
        return create_user_core(user_core_fd, pid, ulimit_c);
    }

cleanup_and_exit:
    if (dd)
        dd_delete(dd);

    if (user_core_fd >= 0)
        unlinkat(dirfd(proc_cwd), core_basename, /*only files*/0);

    if (proc_cwd != NULL)
        closedir(proc_cwd);

    close(pid_proc_fd);

    return err;
}
