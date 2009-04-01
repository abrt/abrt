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

#include "DebugDump.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <string>

#define FILENAME_EXECUTABLE     "executable"
#define FILENAME_CMDLINE        "cmdline"

static void write_log(const char* pid)
{
    openlog("abrt", 0, LOG_DAEMON);
    syslog(LOG_WARNING, "CCpp Language Hook: Crashed pid: %s", pid);
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
    while ((ch = fgetc(fp)) != EOF)
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
    cmdline[ii] = '\0';
    fclose(fp);
    return strdup(cmdline);
}

int main(int argc, char** argv)
{
    const char* program_name = argv[0];
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s: <pid> <signal> <uid>\n",
                program_name);
        return -1;
    }
    const char* pid = argv[1];
    const char* signal = argv[2];
    const char* uid = argv[3];

    if (strcmp(signal, "3") != 0 &&     // SIGQUIT
        strcmp(signal, "4") != 0 &&     // SIGILL
        strcmp(signal, "6") != 0 &&     // SIGABRT
        strcmp(signal, "8") != 0 &&     // SIGFPE
        strcmp(signal, "11") != 0)      // SIGSEGV
    {
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
            throw std::string("Can not get proc info.");
        }

        snprintf(path, sizeof(path), "%s/ccpp-%ld-%s",
                                      DEBUG_DUMPS_DIR, time(NULL), pid);
        dd.Create(path);
        dd.SaveText(FILENAME_ANALYZER, "CCpp");
        dd.SaveText(FILENAME_EXECUTABLE, executable);
        dd.SaveText(FILENAME_UID, uid);
        dd.SaveText(FILENAME_CMDLINE, cmdline);

        snprintf(path + strlen(path), sizeof(path), "/%s",
                                                    FILENAME_BINARYDATA1);

        if ((fp = fopen(path, "w")) == NULL)
        {
            dd.Delete();
            dd.Close();
            throw std::string("Can not open the file ") + path;
        }
        // TODO: rewrite this
        while ((byte = getc(stdin)) != EOF)
        {
            if (putc(byte, fp) == EOF)
            {
                fclose(fp);
                dd.Delete();
                dd.Close();
                throw std::string("Can not write to the file %s.");
            }
        }

        free(executable);
        free(cmdline);
        fclose(fp);
        dd.Close();
        write_log(pid);
    }
    catch (std::string sError)
    {
        fprintf(stderr, "%s: %s\n", program_name, sError.c_str());
        return -2;
    }
    return 0;
}
