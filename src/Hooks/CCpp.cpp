/*
    CCpp.cpp - the hook for C/C++ crashing program

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

/* Make all file ops "large" and make off_t 64-bit.
 * No need to use O_LORGEFILE anywhere
 */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include <syslog.h>

#define FILENAME_EXECUTABLE     "executable"
#define FILENAME_CMDLINE        "cmdline"
#define FILENAME_COREDUMP       "coredump"

#define VAR_RUN_PID_FILE        VAR_RUN"/abrt.pid"

using namespace std;

static char* malloc_readlink(const char *linkname)
{
    char buf[PATH_MAX + 1];
    int len;

    len = readlink(linkname, buf, sizeof(buf)-1);
    if (len >= 0)
    {
        buf[len] = '\0';
        return xstrdup(buf);
    }
    return NULL;
}

static char* get_executable(pid_t pid)
{
    char buf[sizeof("/proc/%u/exe") + sizeof(int)*3];

    sprintf(buf, "/proc/%u/exe", (int)pid);
    return malloc_readlink(buf);
}

static char* get_cwd(pid_t pid)
{
    char buf[sizeof("/proc/%u/cwd") + sizeof(int)*3];

    sprintf(buf, "/proc/%u/cwd", (int)pid);
    return malloc_readlink(buf);
}

static char *append_escaped(char *start, const char *s)
{
    char hex_char_buf[] = "\\x00";

    *start++ = ' ';
    char *dst = start;
    const unsigned char *p = (unsigned char *)s;

    while (1)
    {
        const unsigned char *old_p = p;
        while (*p > ' ' && *p <= 0x7e && *p != '\"' && *p != '\'' && *p != '\\')
            p++;
        if (dst == start)
        {
            if (p != (unsigned char *)s && *p == '\0')
            {
                /* entire word does not need escaping and quoting */
                strcpy(dst, s);
                dst += strlen(s);
                return dst;
            }
            *dst++ = '\'';
        }

        strncpy(dst, s, (p - old_p));
        dst += (p - old_p);

        if (*p == '\0')
        {
            *dst++ = '\'';
            *dst = '\0';
            return dst;
        }
        const char *a;
        switch (*p)
        {
        case '\r': a = "\\r"; break;
        case '\n': a = "\\n"; break;
        case '\t': a = "\\t"; break;
        case '\'': a = "\\\'"; break;
        case '\"': a = "\\\""; break;
        case '\\': a = "\\\\"; break;
        case ' ': a = " "; break;
        default:
            hex_char_buf[2] = "0123456789abcdef"[*p >> 4];
            hex_char_buf[3] = "0123456789abcdef"[*p & 0xf];
            a = hex_char_buf;
        }
        strcpy(dst, a);
        dst += strlen(a);
        p++;
    }
}

// taken from kernel
#define COMMAND_LINE_SIZE 2048
static char* get_cmdline(pid_t pid)
{
    char path[sizeof("/proc/%u/cmdline") + sizeof(int)*3];
    char cmdline[COMMAND_LINE_SIZE];
    char escaped_cmdline[COMMAND_LINE_SIZE*4 + 4];

    escaped_cmdline[1] = '\0';
    sprintf(path, "/proc/%u/cmdline", (int)pid);
    int fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        int len = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (len > 0)
        {
            cmdline[len] = '\0';
            char *src = cmdline;
            char *dst = escaped_cmdline;
            while ((src - cmdline) < len)
            {
                dst = append_escaped(dst, src);
                src += strlen(src) + 1;
            }
        }
    }

    return xstrdup(escaped_cmdline + 1); /* +1 skips extraneous leading space */
}

static int daemon_is_ok()
{
    int fd = open(VAR_RUN_PID_FILE, O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }

    char pid[sizeof(pid_t)*3 + 2];
    int len = read(fd, pid, sizeof(pid)-1);
    close(fd);
    if (len <= 0)
        return 0;

    pid[len] = '\0';
    *strchrnul(pid, '\n') = '\0';
    /* paranoia: we don't want to check /proc//stat or /proc///stat */
    if (pid[0] == '\0' || pid[0] == '/')
        return 0;

    /* TODO: maybe readlink and check that it is "xxx/abrt"? */
    char path[sizeof("/proc/%s/stat") + sizeof(pid)];
    sprintf(path, "/proc/%s/stat", pid);
    struct stat sb;
    if (stat(path, &sb) == -1)
    {
        return 0;
    }

    return 1;
}

int main(int argc, char** argv)
{
    const char* program_name = argv[0];
    if (argc < 4)
    {
        error_msg_and_die("Usage: %s: <dddir> <pid> <signal> <uid>", program_name);
    }
    openlog("abrt", 0, LOG_DAEMON);
    logmode = LOGMODE_SYSLOG;

    const char* dddir = argv[1];
    pid_t pid = atoi(argv[2]);
    const char* signal_str = argv[3];
    int signal = atoi(argv[3]);
    uid_t uid = atoi(argv[4]);

    if (signal != SIGQUIT
     && signal != SIGILL
     && signal != SIGABRT
     && signal != SIGFPE
     && signal != SIGSEGV
    ) {
        /* not an error, exit silently */
        return 0;
    }
    if (pid <= 0 || uid < 0)
    {
        error_msg_and_die("pid '%s' or uid '%s' are bogus", argv[2], argv[4]);
    }

    char *user_pwd = get_cwd(pid); /* may be NULL on error */
    int core_fd = STDIN_FILENO;

    if (!daemon_is_ok())
    {
        /* not an error, exit with exitcode 0 */
        log("abrt daemon is not running. If it crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        goto create_user_core;
    }

    try
    {
        char* executable = get_executable(pid);
        if (executable == NULL)
        {
            error_msg_and_die("can't read /proc/%u/exe link", (int)pid);
        }
        if (strstr(executable, "/hookCCpp"))
        {
            error_msg_and_die("pid %u is '%s', not dumping it to avoid recursion",
                            (int)pid, executable);
        }

        char path[PATH_MAX];

        if (strstr(executable, "/abrtd"))
        {
            /* If abrtd crashes, we don't want to create a _directory_,
             * since that can make new copy of abrtd to process it,
             * and maybe crash again...
             * On the contrary, mere files are ignored by abrtd.
             */
            snprintf(path, sizeof(path), "%s/abrtd-coredump", dddir);
            core_fd = xopen3(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            off_t size = copyfd_eof(STDIN_FILENO, core_fd);
            if (size < 0 || close(core_fd) != 0)
            {
                unlink(path);
                /* copyfd_eof logs the error including errno string,
                 * but it does not log file name */
                error_msg_and_die("error saving coredump to %s", path);
            }
            log("saved core dump of pid %u to %s (%llu bytes)", (int)pid, path, (long long)size);
            return 0;
        }

        char* cmdline = get_cmdline(pid); /* never NULL */
        const char *signame = strsignal(atoi(signal_str));
        char *reason = xasprintf("Process was terminated by signal %s (%s)", signal_str, signame ? signame : signal_str);

        snprintf(path, sizeof(path), "%s/ccpp-%ld-%u", dddir, (long)time(NULL), (int)pid);
        CDebugDump dd;
        dd.Create(path, uid);
        dd.SaveText(FILENAME_ANALYZER, "CCpp");
        dd.SaveText(FILENAME_EXECUTABLE, executable);
        dd.SaveText(FILENAME_CMDLINE, cmdline);
        dd.SaveText(FILENAME_REASON, reason);

        int len = strlen(path);
        snprintf(path + len, sizeof(path) - len, "/"FILENAME_COREDUMP);

        /* We need coredumps to be readable by all, because
         * when abrt daemon processes coredump,
         * process producing backtrace is run under the same UID
         * as the crashed process.
         * Thus 644, not 600 */
        core_fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (core_fd < 0)
        {
            dd.Delete();
            dd.Close();
            perror_msg_and_die("can't open '%s'", path);
        }
        off_t size = copyfd_eof(STDIN_FILENO, core_fd);
        if (size < 0 || fsync(core_fd) != 0)
        {
            unlink(path);
            dd.Delete();
            dd.Close();
            /* copyfd_eof logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("error saving coredump to %s", path);
        }
        lseek(core_fd, 0, SEEK_SET);
        /* note: core_fd is still open, we may use it later to copy core to user's dir */
        free(executable);
        free(cmdline);
        log("saved core dump of pid %u to %s (%llu bytes)", (int)pid, path, (long long)size);
        path[len] = '\0'; /* path now contains directory name */

        /* We close dumpdir before we start catering for crash storm case.
         * Otherwise, delete_debug_dump_dir's from other concurrent
         * CCpp's won't be able to delete our dump (their delete_debug_dump_dir
         * will wait for us), and we won't be able to delete their dumps.
         * Classic deadlock.
         */
        dd.Close();

        /* Parse abrt.conf and plugins/CCpp.conf */
        unsigned setting_MaxCrashReportsSize = 0;
        bool setting_MakeCompatCore = false;
        bool abrt_conf = true;
        FILE *fp = fopen(CONF_DIR"/abrt.conf", "r");
        if (fp)
        {
            char line[256];
            while (1)
            {
		if (fgets(line, sizeof(line), fp) == NULL)
		{
		    /* Next .conf file plz */
		    if (abrt_conf)
		    {
			abrt_conf = false;
			fp = fopen(CONF_DIR"/plugins/CCpp.conf", "r");
			if (fp)
			    continue;
		    }
		    break;
		}

                unsigned len = strlen(line);
                if (len > 0 && line[len-1] == '\n')
                    line[--len] = '\0';
                const char *p = skip_whitespace(line);
#undef DIRECTIVE
#define DIRECTIVE "MaxCrashReportsSize"
                if (strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
                {
                    p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
                    if (*p != '=')
                        continue;
                    p = skip_whitespace(p + 1);
                    if (isdigit(*p))
                        /* x1.25: go a bit up, so that usual in-daemon trimming
                         * kicks in first, and we don't "fight" with it. */
                        setting_MaxCrashReportsSize = (unsigned long)atoi(p) * 5 / 4;
                    continue;
                }
#undef DIRECTIVE
#define DIRECTIVE "MakeCompatCore"
                if (strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)
                {
                    p = skip_whitespace(p + sizeof(DIRECTIVE)-1);
                    if (*p != '=')
                        continue;
                    p = skip_whitespace(p + 1);
                    setting_MakeCompatCore = string_to_bool(p);
                    continue;
                }
#undef DIRECTIVE
                /* add more 'if (strncmp(p, DIRECTIVE, sizeof(DIRECTIVE)-1) == 0)' here... */
            }
            fclose(fp);
        }

        /* rhbz#539551: "abrt going crazy when crashing process is respawned".
         * Do we want to protect against or ameliorate this? How? Ideas:
         * (1) nice ourself?
         * (2) check total size of dump dir, if it overflows, either abort dump
         *     or delete oldest/biggest dumps? [abort would be simpler,
         *     abrtd already does "delete on overflow" thing]
         * (3) detect parallel dumps in progress and back off
         *     (pause/renice further/??)
         */

        if (setting_MaxCrashReportsSize > 0)
        {
            string worst_dir;
            while (1)
            {
                const char *base_dirname = strrchr(path, '/') + 1; /* never NULL */
                /* We exclude our own dump from candidates for deletion (3rd param): */
                double dirsize = get_dirsize_find_largest_dir(DEBUG_DUMPS_DIR, &worst_dir, base_dirname);
                if (dirsize / (1024*1024) < setting_MaxCrashReportsSize || worst_dir == "")
                    break;
                log("Size of '%s' >= %u MB, deleting '%s'", DEBUG_DUMPS_DIR, setting_MaxCrashReportsSize, worst_dir.c_str());
                delete_debug_dump_dir(concat_path_file(DEBUG_DUMPS_DIR, worst_dir.c_str()).c_str());
                worst_dir = "";
            }
        }

        if (!setting_MakeCompatCore)
            return 0;
        /* fall through to creating user core */
    }
    catch (CABRTException& e)
    {
        error_msg_and_die("%s", e.what());
    }
    catch (std::exception& e)
    {
        error_msg_and_die("%s", e.what());
    }

 create_user_core:
    /* Write a core file for user */

    /* man core:
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
     * [shouldn't it be checked by kernel? 2.6.30.9-96 doesn't, still
     * calls us even if "ulimit -c 0"]
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

    errno = 0;
    if (setuid(uid) != 0
     || user_pwd == NULL
     || chdir(user_pwd) != 0
    ) {
        perror_msg_and_die("can't cd to %s", user_pwd);
    }

    /* Mimic "core.PID" if requested */
    char core_basename[sizeof("core.%u") + sizeof(int)*3] = "core";
    char buf[] = "0\n";
    int fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
    if (fd >= 0)
    {
	read(fd, buf, sizeof(buf));
	close(fd);
    }
    if (strcmp(buf, "1\n") == 0)
    {
	sprintf(core_basename, "core.%u", (int)pid);
    }

    /* Do not O_TRUNC: if later checks fail, we do not want to have file already modified here */
    errno = 0;
    int usercore_fd = open(core_basename, O_WRONLY | O_CREAT | O_NOFOLLOW, 0600); /* kernel makes 0600 too */
    struct stat sb;
    if (usercore_fd < 0
     || fstat(usercore_fd, &sb) != 0
     || !S_ISREG(sb.st_mode)
     || sb.st_nlink != 1
    /* || kernel internal dumper checks this too: if (inode->i_uid != current->fsuid) <fail>, need to mimic? */
    ) {
        perror_msg("%s/%s is not a regular file with link count 1", user_pwd, core_basename);
        return 1;
    }

    if (ftruncate(usercore_fd, 0) != 0
     || copyfd_eof(core_fd, usercore_fd) < 0
     || close(usercore_fd) != 0
    ) {
        perror_msg("write error writing %s/%s", user_pwd, core_basename);
        unlink(core_basename);
        return 1;
    }
    log("saved core dump of pid %u to %s/%s", (int)pid, user_pwd, core_basename);

    return 0;
}
