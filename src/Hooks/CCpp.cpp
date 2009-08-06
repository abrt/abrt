/*
    CCpp.cpp - the hook for C/C++ crashing program

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

#include "DebugDump.h"
#include "ABRTException.h"
#include <syslog.h>
#include <string>

#define FILENAME_EXECUTABLE     "executable"
#define FILENAME_CMDLINE        "cmdline"
#define FILENAME_COREDUMP       "coredump"

#define VAR_RUN_PID_FILE         VAR_RUN"/abrt.pid"

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

// taken from kernel
#define COMMAND_LINE_SIZE 2048

static char* get_cmdline(pid_t pid)
{
    char path[PATH_MAX];
    char cmdline[COMMAND_LINE_SIZE];
    snprintf(path, sizeof(path), "/proc/%u/cmdline", (int)pid);
    int dst = 0;

    int fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        int len = read(fd, cmdline, sizeof(cmdline) - 1);
        if (len >= 0)
        {
            int src = 0;
            while (src < len)
            {
                char ch = cmdline[src++];
                if (ch == '\0')
                {
                    cmdline[dst++] = ' ';
                }
                /* TODO: maybe just ch >= ' '? */
                else if (isspace(ch) || (isascii(ch) && !iscntrl(ch)))
                {
                    cmdline[dst++] = ch;
                }
            }
        }
        close(fd);
    }
    cmdline[dst] = '\0';

    return xstrdup(cmdline);
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
        char* executable;
        char* cmdline;
        executable = get_executable(pid);
        cmdline = get_cmdline(pid);
        if (executable == NULL || cmdline == NULL)
        {
            error_msg_and_die("can not get proc info for pid %u", (int)pid);
        }
        if (strstr(executable, "/abrt"))
        {
            /* free(executable); - why bother? */
            /* free(cmdline); */
            error_msg_and_die("pid %u is '%s', not dumping it to avoid abrt recursion",
                            (int)pid, executable);
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/ccpp-%ld-%u", dddir, (long)time(NULL), (int)pid);

        CDebugDump dd;
        dd.Create(path, uid);
        dd.SaveText(FILENAME_ANALYZER, "CCpp");
        dd.SaveText(FILENAME_EXECUTABLE, executable);
        dd.SaveText(FILENAME_CMDLINE, cmdline);
        dd.SaveText(FILENAME_REASON, std::string("Process was terminated by signal ") + signal_str);

        snprintf(path + strlen(path), sizeof(path), "/%s", FILENAME_COREDUMP);

        int fd;
        /* We need coredumps to be readable by all, because
         * process producing backtraces is run under the same UID
         * as the crashed process.
	 * Thus 644, not 600 */
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            dd.Delete();
            dd.Close();
            perror_msg_and_die("can't open '%s'", path);
        }
        if (copyfd_eof(STDIN_FILENO, fd) < 0)
        {
            /* close(fd); - why bother? */
            dd.Delete();
            dd.Close();
            /* copyfd_eof logs the error including errno string,
             * but it does not log file name */
            error_msg_and_die("error saving coredump to %s", path);
        }
        /* close(fd); - why bother? */
        /* free(executable); */
        /* free(cmdline); */
        dd.Close();
        log("saved core dump of pid %u to %s", (int)pid, path);
    }
    catch (CABRTException& e)
    {
        error_msg_and_die("%s", e.what().c_str());
    }
    catch (std::exception& e)
    {
        error_msg_and_die("%s", e.what());
    }
    return 0;
}
