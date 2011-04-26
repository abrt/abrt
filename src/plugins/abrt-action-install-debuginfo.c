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
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// TODO: honor configure --prefix here:
#define EXECUTABLE "/usr/bin/abrt-action-install-debuginfo.py"

static void error_msg_and_die(const char *msg, const char *arg)
{
    write(2, msg, strlen(msg));
    if (arg)
    {
        write(2, " '", 2);
        write(2, msg, strlen(msg));
        write(2, "'", 1);
    }
    write(2, "\n", 1);
    exit(1);
}


/* A binary wrapper is needed around python scripts if we want
 * to run them in sgid/suid mode.
 *
 * This is such a wrapper.
 */
int main(int argc, char **argv)
{
    /*
     * We disallow passing of arguments which point to writable dirs.
     * This way, the script will always use default arguments.
     */
    char **pp = argv;
    char *arg;
    while ((arg = *++pp) != NULL)
    {
        if (strncmp(arg, "--cache", 7) == 0)
            error_msg_and_die("bad option", arg);
        if (strncmp(arg, "--tmpdir", 8) == 0)
            error_msg_and_die("bad option", arg);
        if (strncmp(arg, "-i", 2) == 0)
            error_msg_and_die("bad option", arg);
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
        setreuid(u, u);

    /* We use full path, and execv instead of execvp in order to
     * disallow user to execute his own abrt-action-install-debuginfo.py
     * in his dir by setting up corresponding malicious $PATH.
     */
    execv(EXECUTABLE, argv);
    error_msg_and_die("Can't execute", EXECUTABLE);
}
