/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#include "libabrt.h"

#define EXECUTABLE "abrt-action-install-debuginfo"

/* A binary wrapper is needed around python scripts if we want
 * to run them in sgid/suid mode.
 *
 * This is such a wrapper.
 */
int main(int argc, char **argv)
{
    /*
     * We disallow passing of arguments which point to writable dirs
     * and other files possibly not accessible to calling user.
     * This way, the script will always use default values for these arguments.
     */
    char **pp = argv;
    char *arg;
    while ((arg = *++pp) != NULL)
    {
        /* Allow taking ids from stdin */
        if (strcmp(arg, "--ids=-") == 0)
            continue;

        if (strncmp(arg, "--exact", 7) == 0)
            continue;

        if (strncmp(arg, "--cache", 7) == 0)
            error_msg_and_die("bad option %s", arg);
        if (strncmp(arg, "--tmpdir", 8) == 0)
            error_msg_and_die("bad option %s", arg);
        if (strncmp(arg, "--ids", 5) == 0)
            error_msg_and_die("bad option %s", arg);
    }

    /* Switch real user/group to effective ones.
     * Otherwise yum library gets confused - gets EPERM (why??).
     */
    gid_t g = getegid();
    /* do setregid only if we have to, to not upset selinux needlessly */
    if (g != getgid())
        setregid(g, g);
    uid_t u = geteuid();
    if (u != getuid())
    {
        setreuid(u, u);
        /* We are suid'ed! */
        /* Prevent malicious user from messing up with suid'ed process: */
#if 1
// We forgot to sanitize PYTHONPATH. And who knows what else we forgot
// (especially considering *future* new variables of this kind).
// We switched to clearing entire environment instead:
        clearenv();
#else
        /* Clear dangerous stuff from env */
        static const char forbid[] =
            "LD_LIBRARY_PATH" "\0"
            "LD_PRELOAD" "\0"
            "LD_TRACE_LOADED_OBJECTS" "\0"
            "LD_BIND_NOW" "\0"
            "LD_AOUT_LIBRARY_PATH" "\0"
            "LD_AOUT_PRELOAD" "\0"
            "LD_NOWARN" "\0"
            "LD_KEEPDIR" "\0"
        ;
        const char *p = forbid;
        do {
            unsetenv(p);
            p += strlen(p) + 1;
        } while (*p);
#endif
        /* Set safe PATH */
// TODO: honor configure --prefix here by adding it to PATH
// (otherwise abrt-action-install-debuginfo would fail to spawn abrt-action-trim-files):
        if (u == 0)
            putenv((char*) "PATH=/usr/sbin:/sbin:/usr/bin:/bin");
        else
            putenv((char*) "PATH=/usr/bin:/bin");
    }

    execvp(EXECUTABLE, argv);
    error_msg_and_die("Can't execute %s", EXECUTABLE);
}
