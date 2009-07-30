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
//#include <stdlib.h>
//#include <string.h>
//#include <limits.h>
//#include <stdio.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <unistd.h>
//#include <time.h>
#include <syslog.h>
#include <string>

#define FILENAME_EXECUTABLE     "executable"
#define FILENAME_CMDLINE        "cmdline"
#define FILENAME_COREDUMP       "coredump"

#define VAR_RUN_PID_FILE         VAR_RUN"/abrt.pid"

static void write_success_log(const char* pid)
{
    openlog("abrt", 0, LOG_DAEMON);
    syslog(LOG_WARNING, "CCpp Language Hook: Crashed pid: %s", pid);
    closelog();
}

static void write_faliure_log(const char* msg)
{
    openlog("abrt", 0, LOG_DAEMON);
    syslog(LOG_WARNING, "CCpp Language Hook: Exception occur: %s", msg);
    closelog();
}


char* get_executable(const char* pid)
{
    char path[PATH_MAX];
    char executable[PATH_MAX];
    int len;

    snprintf(path, sizeof(path), "/proc/%s/exe", pid);
    if ((len = readlink(path, executable, PATH_MAX)) != -1)
    {
        executable[len] = '\0';
        return strdup(executable);
    }
    return NULL;
}

// taken from kernel
#define COMMAND_LINE_SIZE 2048

char* get_cmdline(const char* pid)
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
    return strdup(cmdline);
}

#define PID_MAX                 16

int daemon_is_ok()
{
    char pid[PID_MAX];
    char path[PATH_MAX];
    struct stat buff;
    FILE* fp = fopen(VAR_RUN_PID_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }
    fgets(pid, sizeof(pid), fp);
    fclose(fp);
    *strchrnul(pid, '\n') = '\0';
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
        fprintf(stderr, "Usage: %s: <dddir> <pid> <signal> <uid>\n",
                program_name);
        return -1;
    }
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
        FILE* fp;
        CDebugDump dd;
        int byte;
        char path[PATH_MAX];
        char* executable = NULL;
        char* cmdline = NULL;

        executable = get_executable(pid);
        cmdline = get_cmdline(pid);

        if (executable == NULL ||
            cmdline == NULL)
        {
            free(executable);
            free(cmdline);
            throw CABRTException(EXCEP_FATAL, "Can not get proc info.");
        }

        snprintf(path, sizeof(path), "%s/ccpp-%ld-%s", dddir, time(NULL), pid);
        dd.Create(path, uid);
        dd.SaveText(FILENAME_ANALYZER, "CCpp");
        dd.SaveText(FILENAME_EXECUTABLE, executable);
        dd.SaveText(FILENAME_CMDLINE, cmdline);
        dd.SaveText(FILENAME_REASON, std::string("Process was terminated by signal ") + signal);

        snprintf(path + strlen(path), sizeof(path), "/%s", FILENAME_COREDUMP);

        if ((fp = fopen(path, "w")) == NULL)
        {
            dd.Delete();
            dd.Close();
            throw CABRTException(EXCEP_FATAL, std::string("Can not open the file ") + path);
        }
        // TODO: rewrite this
        while ((byte = getc(stdin)) != EOF)
        {
            if (putc(byte, fp) == EOF)
            {
                fclose(fp);
                dd.Delete();
                dd.Close();
                throw CABRTException(EXCEP_FATAL, "Can not write to the file %s.");
            }
        }

        free(executable);
        free(cmdline);
        fclose(fp);
        dd.Close();
        write_success_log(pid);
    }
    catch (CABRTException& e)
    {
        fprintf(stderr, "%s: %s\n", program_name, e.what().c_str());
        write_faliure_log(e.what().c_str());
        return -2;
    }
    catch (std::exception& e)
    {
        fprintf(stderr, "%s: %s\n", program_name, e.what());
        write_faliure_log(e.what());
        return -2;
    }
    return 0;
}
