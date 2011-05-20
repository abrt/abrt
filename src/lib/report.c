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
#include "report.h"

static int run_reporter_ui(char **args, int flags)
{
    const char *path;
    /*
    if is isatty
      -> run cli reporter
      path = "cli"
    */

    pid_t pid = vfork();
    if (pid == 0)
    {
        /* Some callers set SIGCHLD to SIG_IGN.
         * However, reporting spawns child processes.
         * Suppressing child death notification terribly confuses some of them.
         * Just in case, undo it.
         * Note that we do it in the child, so the parent is never affected.
         */
        signal(SIGCHLD, SIG_DFL);
        path = BIN_DIR"/bug-reporting-wizard";
        VERB1 log("Executing: %s", path);
        execv(path, args);
        /* Did not find the desired executable in the installation directory.
         * Trying to find it in PATH
         */
        path = "bug-reporting-wizard";
        execvp(path, args);
        perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
    }
    else if(pid > 0)
    {
        if (flags & WAIT)
        {
            int status;
            pid_t p = waitpid(pid, &status, 0);
            if(p <= 0)
            {
                perror_msg("can't waitpid");
                return EXIT_FAILURE;
            }
            if (WIFEXITED(status))
            {
                VERB2 log("reporting finished with exitcode: status=%d\n", WEXITSTATUS(status));
                return WEXITSTATUS(status);
            }
            else // (WIFSIGNALED(status))
            {
                VERB2 log("reporting killed by signal %d\n", WTERMSIG(status));
                return WTERMSIG(status) + 128;
            }
        }
    }
    return 0;
}

int analyze_and_report_dir(const char* dirname, int flags)
{
    char *args[4];

    args[0] = (char *)"bug-reporting-wizard";
    args[1] = (char *)"--";
    args[2] = (char *)dirname;
    args[3] = NULL;

    run_reporter_ui(args, flags);
    return 0;
}

/* analyzes AND reports a problem saved on disk
 * - takes user through all the steps in reporting wizard
 */
int analyze_and_report(problem_data_t *pd, int flags)
{
    int result = 0;
    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, "/tmp"/* /var/tmp ?? */);
    if (!dd)
        return -1;
    char *dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'\n", dir_name);
    result = analyze_and_report_dir(dir_name, flags);

    /* if we wait for reporter to finish, we can clean the tmp dir
     * and we should reload the problem data, so caller doesn't see the stalled
     * data
    */
    if (flags & WAIT)
    {
        g_hash_table_remove_all(pd);
        dd = dd_opendir(dir_name, 0);
        if (dd)
        {
            load_problem_data_from_dump_dir(pd, dd);
            dd_delete(dd);
        }
    }
    free(dir_name);
    return result;
}

/* report() and report_dir() don't take flags, because in all known use-cases
 * it doesn't make sense to not wait for the result
 *
*/

/* reports a problem saved on disk
 * - shows only reporter selector and progress
*/
int report_dir(const char* dirname)
{
    char *args[5];

    args[0] = (char *)"bug-reporting-wizard";
    args[1] = (char *)"--report-only";
    args[2] = (char *)"--";
    args[3] = (char *)dirname;
    args[4] = NULL;

    int flags = WAIT;
    int status;
    status = run_reporter_ui(args, flags);
    return status;
}

int report(problem_data_t *pd)
{
    /* adds:
     *  analyzer:libreport
     *  executable:readlink(/proc/<pid>/exe)
     * tries to guess component
    */
    add_basics_to_problem_data(pd);
    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, "/tmp"/* /var/tmp ?? */);
    if (!dd)
        return -1;
    dd_create_basic_files(dd, getuid());
    char *dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'\n", dir_name);
    int result = report_dir(dir_name);

    /* here we always wait for reporter to finish, we can clean the tmp dir
     * and we should reload the problem data, so caller doesn't see the stalled
     * data
    */
    g_hash_table_remove_all(pd); //what if something fails? is it ok to return empty pd rather then stalled pd?
    dd = dd_opendir(dir_name, 0);
    if (dd)
    {
        load_problem_data_from_dump_dir(pd, dd);
        dd_delete(dd);
    }
    free(dir_name);

    return result;
}
