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
#include <unistd.h>

#define CORESTEP (1024)

int main(int argc, char** argv)
{
    const char* program_name = argv[0];
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s: <pid> <time> <signal>\n",
                program_name);
        return -1;
    }
    const char* pid = argv[1];
    const char* time = argv[2];
    const char* signal = argv[3];

    if (strcmp(signal, "11") != 0)
    {
        return 0;
    }

    char path[PATH_MAX];
    CDebugDump dd;
    snprintf(path, sizeof(path), "%s/%s%s", DEBUG_DUMPS_DIR, time, pid);
    try
    {
        dd.Create(path);
        dd.SaveText(FILENAME_TIME, time);
        dd.SaveText(FILENAME_LANGUAGE, "CCpp");
        dd.SaveProc(pid);

        int size = CORESTEP*sizeof(char);
        int ii = 0;
        int data = 0;
        char* core = NULL;
        if ((core = (char*)malloc(size)) == NULL)
        {
            fprintf(stderr, "%s: not enaught memory.\n", program_name);
            perror("");
            return -3;
        }
        while ((data = getc(stdin)) != EOF)
        {
            if (ii >= size)
            {
                size *= CORESTEP*sizeof(char);
                if ((core = (char*)realloc(core, size)) == NULL)
                {
                    fprintf(stderr, "%s: not enaught memory.\n", program_name);
                    perror("");
                    return -3;
                }
            }
            core[ii] = data;
            ii++;
        }
        dd.SaveBinary(FILENAME_BINARYDATA1, core, ii);
        free(core);
    }
    catch (std::string sError)
    {
        fprintf(stderr, "%s: %s\n", program_name, sError.c_str());
        return -2;
    }

    return 0;
}
