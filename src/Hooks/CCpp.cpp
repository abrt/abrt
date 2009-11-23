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
#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include <syslog.h>

#define FILENAME_EXECUTABLE     "executable"
#define FILENAME_CMDLINE        "cmdline"
#define FILENAME_COREDUMP       "coredump"

#define VAR_RUN_PID_FILE        VAR_RUN"/abrt.pid"

static char* get_executable(pid_t pid)
{
    char buf[PATH_MAX + 1];
    int len;

    snprintf(buf, sizeof(buf), "/proc/%u/exe", (int)pid);
    len = readlink(buf, buf, sizeof(buf)-1);
    if (len >= 0)
    {
        buf[len] = '\0';
        return xstrdup(buf);
    }
    return NULL;
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
    char pid[sizeof(pid_t)*3 + 2];
    char path[PATH_MAX];
    struct stat buff;
    int fd = open(VAR_RUN_PID_FILE, O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }
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
    snprintf(path, sizeof(path), "/proc/%s/stat", pid);
    if (stat(path, &buff) == -1)
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
    if (!daemon_is_ok())
    {
        /* not an error, exit with exitcode 0 */
        log("abrt daemon is not running. If it crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        return 0;
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
            int fd = xopen3(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            off_t size = copyfd_eof(STDIN_FILENO, fd);
            if (size < 0 || close(fd) != 0)
            {
                unlink(path);
                /* copyfd_eof logs the error including errno string,
                 * but it does not log file name */
                error_msg_and_die("error saving coredump to %s", path);
            }
            log("saved core dump of pid %u to %s (%llu bytes)", (int)pid, path, (long long)size);
            return 0;
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
         * process producing backtraces is run under the same UID
         * as the crashed process.
         * Thus 644, not 600 */
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            dd.Delete();
            dd.Close();
            perror_msg_and_die("can't open '%s'", path);
        }
        off_t size = copyfd_eof(STDIN_FILENO, fd);
        if (size < 0 || close(fd) != 0)
        {
            unlink(path);
            dd.Delete();
            dd.Close();
            /* copyfd_eof logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("error saving coredump to %s", path);
        }
        /* free(executable); - why bother? */
        /* free(cmdline); */
        log("saved core dump of pid %u to %s (%llu bytes)", (int)pid, path, (long long)size);
        return 0;
    }
    catch (CABRTException& e)
    {
        error_msg_and_die("%s", e.what());
    }
    catch (std::exception& e)
    {
        error_msg_and_die("%s", e.what());
    }
    return 0;
}
