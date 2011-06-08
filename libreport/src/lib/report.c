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

#include "libreport.h"
#include "report.h"

int report_problem_in_dir(const char *dirname, int flags)
{
    const char *path;
    /*
    if is isatty
      -> run cli reporter
      path = "cli"
    */

    char *args[5], **pp;
    pp = args;
    *pp++ = (char *)"bug-reporting-wizard";
    if (!(flags & LIBREPORT_ANALYZE))
        *pp++ = (char *)"--report-only";
    *pp++ = (char *)"--";
    *pp++ = (char *)dirname;
    *pp++ = NULL;

    pid_t pid = vfork();
    if (pid < 0) /* error */
    {
        perror_msg("vfork");
        return -1;
    }

    if (pid == 0) /* child */
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
        perror_msg_and_die("Can't execute %s", path);
    }

    /* parent */
    if (flags & LIBREPORT_WAIT)
    {
        int status;
        pid_t p = waitpid(pid, &status, 0);
        if (p <= 0)
        {
            perror_msg("can't waitpid");
            return -1;
        }
        if (WIFEXITED(status))
        {
            VERB2 log("reporting finished with exitcode %d", WEXITSTATUS(status));
            return WEXITSTATUS(status);
        }
        /* child died from a signal: WIFSIGNALED(status) should be true */
        VERB2 log("reporting killed by signal %d", WTERMSIG(status));
        return WTERMSIG(status) + 128;
    }

    return 0;
}

int report_problem_in_memory(problem_data_t *pd, int flags)
{
    int result = 0;
    struct dump_dir *dd = create_dump_dir_from_problem_data(pd, "/tmp"/* /var/tmp ?? */);
    if (!dd)
        return -1;
    char *dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'", dir_name);

// TODO: if !LIBREPORT_WAIT pass LIBREPORT_DEL_DIR, and teach bug-reporting-wizard
// an option to delete directory after reporting?
// It will make !LIBREPORT_WAIT reporting possible
    result = report_problem_in_dir(dir_name, flags);

    /* If we wait for reporter to finish, we should clean the tmp dir.
     * We can also reload the problem data if requested.
     */
    if (flags & LIBREPORT_WAIT)
    {
        if (flags & LIBREPORT_RELOAD_DATA)
            g_hash_table_remove_all(pd);
        dd = dd_opendir(dir_name, 0);
        if (dd)
        {
            if (flags & LIBREPORT_RELOAD_DATA)
                load_problem_data_from_dump_dir(pd, dd, NULL);
            dd_delete(dd);
        }
    }

    free(dir_name);
    return result;
}

int report_problem(problem_data_t *pd)
{
    return report_problem_in_memory(pd, LIBREPORT_WAIT);
}
