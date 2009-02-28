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

#define CORESTEP (1024)

static void write_log(const char* pid)
{
    openlog("crash-catcher", 0, LOG_DAEMON);
    syslog(LOG_WARNING, "CCpp Language Hook: Crashed pid: %s", pid);
    closelog();
}

int main(int argc, char** argv)
{
    const char* program_name = argv[0];
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s: <pid> <signal>\n",
                program_name);
        return -1;
    }
    const char* pid = argv[1];
    const char* signal = argv[2];

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
        char dd_path[PATH_MAX] = DEBUG_DUMPS_DIR;
        char cd_path[PATH_MAX];

        snprintf(dd_path, sizeof(dd_path), "%s/ccpp-%ld-%s",
                                            DEBUG_DUMPS_DIR, time(NULL), pid);
        snprintf(cd_path, sizeof(cd_path), "%s/%s",
                                           dd_path, FILENAME_BINARYDATA1);

        dd.Create(dd_path);
        dd.SaveProc(pid);
        dd.SaveText(FILENAME_LANGUAGE, "CCpp");
        if ((fp = fopen(cd_path, "w")) == NULL)
        {
            fprintf(stderr, "%s: Can not open the file %s.\n",
                            program_name, cd_path);
            dd.Delete();
            dd.Close();
            return -2;
        }
        // TODO: rewrite this
        while ((byte = getc(stdin)) != EOF)
        {
            if (putc(byte, fp) == EOF)
            {
                fprintf(stderr, "%s: Can not write to the file %s.\n",
                                program_name, cd_path);
                fclose(fp);
                dd.Delete();
                dd.Close();
                return -3;
            }
        }
        fclose(fp);
        dd.Close();
        write_log(pid);
    }
    catch (std::string sError)
    {
        fprintf(stderr, "%s: %s\n", program_name, sError.c_str());
        return -4;
    }
    return 0;
}
