/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
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

#include "CrashWatcher.h"
#include <iostream>
#include <cstdio>

CCrashWatcher *g_pCrashWatcher = NULL;

void terminate(int signal)
{
    fprintf(stderr, "Got SIGINT/SIGTERM, cleaning up..\n");
    delete g_pCrashWatcher;
    exit(0);
}

void print_help()
{
    
}

int main(int argc, char** argv)
{
    int daemonize = 1;
    /*signal handlers */
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);
    try
    {
        g_pCrashWatcher = new CCrashWatcher(DEBUG_DUMPS_DIR);

        if (argc > 1)
        {
            if (strcmp(argv[1], "-d") == 0)
            {
                daemonize = 0;
            }
        }
        if(daemonize)
        {
            g_pCrashWatcher->Daemonize();
        }
        else
        {
            g_pCrashWatcher->Run();
        }
    }
    catch(std::exception& e)
    {
        std::cerr << "Cannot create daemon: " << e.what() << std::endl;
    }
}

