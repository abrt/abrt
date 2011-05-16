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

#include "abrtlib.h"

int analyze_and_report_dir(const char* dirname)
{
    /*
    if is isatty -> run cli reporter
    */
    pid_t pid = vfork();
    if (pid == 0)
    {
        /* Some callers set SIGCHLD to SIG_IGN.
         * However, reporting spawns chils processes.
         * Suppressing chil death notification terribly confuses some of them.
         * Just in case, undo it.
         * Note that we do it in the child, so the parent is never affected.
         */
        signal(SIGCHLD, SIG_DFL); // applet still set it to SIG_IGN
        VERB1 log("Executing: %s %s", "bug-reporting-wizard", dirname);
        execl(BIN_DIR"/bug-reporting-wizard", "bug-reporting-wizard", "--", dirname, NULL);
        // note the -o in options which means --report-only
        /* Did not find abrt-gui in installation directory. Oh well */
        /* Trying to find it in PATH */
        execlp("bug-reporting-wizard", "bug-reporting-wizard", "--", dirname, NULL);
        perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
    }
    return 0;
}

/* analyzes AND reports a problem saved on disk
 * - takes user through all the steps in reporting wizard
 */
int analyze_and_report(problem_data_t *pd)
{
    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, "/tmp"/* /var/tmp ?? */);
    if (!dd)
        return -1;
    char *dir_name = strdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'\n", dir_name);
    analyze_and_report_dir(dir_name);
    free(dir_name);
    return 0;
}

/* reports a problem saved on disk
 * - shows only reporter selector and progress
*/
int report_dir(const char* dirname)
{
    pid_t pid = vfork();
    if (pid == 0)
    {
        /* Some callers set SIGCHLD to SIG_IGN.
         * However, reporting spawns chils processes.
         * Suppressing chil death notification terribly confuses some of them.
         * Just in case, undo it.
         * Note that we do it in the child, so the parent is never affected.
         */
        signal(SIGCHLD, SIG_DFL); // applet still set it to SIG_IGN
        VERB1 log("Executing: %s %s", "bug-reporting-wizard", dirname);
        execl(BIN_DIR"/bug-reporting-wizard", "bug-reporting-wizard",
                  "-o", "--", dirname, NULL);
        // note the -o in options which means --report-only
        /* Did not find abrt-gui in installation directory. Oh well */
        /* Trying to find it in PATH */
        execlp("bug-reporting-wizard", "bug-reporting-wizard",
                "-o", "--", dirname, NULL);
        perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
    }
    return 0;
}

int report(problem_data_t *pd)
{
    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, "/tmp"/* /var/tmp ?? */);
    if (!dd)
        return -1;
    char *dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'\n", dir_name);
    report_dir(dir_name);
    free(dir_name);

    return 0;
}

