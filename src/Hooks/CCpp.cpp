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

static char* get_executable(const char* pid)
{
    char buf[PATH_MAX + 1];
    int len;

    snprintf(buf, sizeof(buf), "/proc/%s/exe", pid);
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

static char* get_cmdline(const char* pid)
{
    char path[PATH_MAX];
    char cmdline[COMMAND_LINE_SIZE];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);
    FILE* fp = fopen(path, "r");
    int ch;
    int ii = 0;
    if (fp)
    {
        while ((ch = fgetc(fp)) != EOF && ii < COMMAND_LINE_SIZE-1)
        {
            if (ch == 0)
            {
                cmdline[ii] = ' ';
            }
            else if (isspace(ch) || (isascii(ch) && !iscntrl(ch)))
            {
                cmdline[ii] = ch;
            }
            ii++;
        }
        fclose(fp);
    }
    cmdline[ii] = '\0';
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
    const char* pid = argv[2];
    const char* signal = argv[3];
    const char* uid = argv[4];

    if (strcmp(signal, "3") != 0 &&     // SIGQUIT
        strcmp(signal, "4") != 0 &&     // SIGILL
        strcmp(signal, "6") != 0 &&     // SIGABRT
        strcmp(signal, "8") != 0 &&     // SIGFPE
        strcmp(signal, "11") != 0)      // SIGSEGV
    {
        return 0;
    }
    if (!daemon_is_ok())
    {
        log("abrt daemon is not running. If it crashed, "
            "/proc/sys/kernel/core_pattern contains a stale value, "
            "consider resetting it to 'core'"
        );
        return 0;
    }

    try
    {
        int fd;
        CDebugDump dd;
        char path[PATH_MAX];
        char* executable;
        char* cmdline;

        executable = get_executable(pid);
        cmdline = get_cmdline(pid);
        if (executable == NULL || cmdline == NULL)
        {
            /* free(executable); - why bother? */
            /* free(cmdline); */
            error_msg_and_die("can not get proc info for pid %s", pid);
        }

        snprintf(path, sizeof(path), "%s/ccpp-%ld-%s", dddir, (long)time(NULL), pid);
        dd.Create(path, uid);
        dd.SaveText(FILENAME_ANALYZER, "CCpp");
        dd.SaveText(FILENAME_EXECUTABLE, executable);
        dd.SaveText(FILENAME_CMDLINE, cmdline);
        dd.SaveText(FILENAME_REASON, std::string("Process was terminated by signal ") + signal);

        snprintf(path + strlen(path), sizeof(path), "/%s", FILENAME_COREDUMP);

        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
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
        log("saved core dump of pid %s to %s", pid, path);
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
