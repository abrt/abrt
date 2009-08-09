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
#include "ABRTException.h"
#include <iostream>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

uint8_t sig_caught;

static void handle_fatal_signal(int signal)
{
    sig_caught = signal;
}

CCrashWatcher *g_pCrashWatcher = NULL;

void print_help()
{

}

int main(int argc, char** argv)
{
    int daemonize = 1;

    /* signal handlers */
    signal(SIGTERM, handle_fatal_signal);
    signal(SIGINT, handle_fatal_signal);

    try
    {
        if (argv[1])
        {
            if (strcmp(argv[1], "-d") == 0)
            {
                daemonize = 0;
            }
        }
        if (daemonize)
        {
            /* Open stdin to /dev/null. We do it before forking
             * in order to emit useful exitcode to the parent
             * if open fails */
            close(STDIN_FILENO);
            if (open("/dev/null", O_RDWR))
            {
                throw CABRTException(EXCEP_FATAL, "Can't open /dev/null");
            }
            /* forking to background */
            pid_t pid = fork();
            if (pid < 0)
            {
                throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Daemonize(): Fork error");
            }
            if (pid > 0)
            {
                /* Parent */
                /* Wait for child to notify us via SIGTERM that it feels ok */
                int i = 20; /* 2 sec */
                while (sig_caught == 0 && --i)
                {
                        usleep(100 * 1000);
                }
                _exit(sig_caught != SIGTERM); /* TERM:ok(0), else:bad(1) */
            }
            /* Child (daemon) continues */
            pid_t sid = setsid();
            if(sid == -1)
            {
                throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Daemonize(): setsid failed");
            }
            /* We must not leave fds 0,1,2 closed.
             * Otherwise fprintf(stderr) dumps messages into random fds, etc. */
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            dup(0);
            dup(0);
        }
        g_pCrashWatcher = new CCrashWatcher(DEBUG_DUMPS_DIR);
        if (daemonize)
        {
            /* Let parent know we initialized ok */
            kill(getppid(), SIGTERM);
        }
        g_pCrashWatcher->Run();
    }
    catch (CABRTException& e)
    {
        std::cerr << "Cannot create daemon: " << e.what() << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "Cannot create daemon: " << e.what() << std::endl;
    }

    delete g_pCrashWatcher;

    /* Take care to emit correct exit status */
    if (sig_caught) {
        signal(sig_caught, SIG_DFL);
        raise(sig_caught);
    }
    /* I think we never end up here */
    return 0;
}
