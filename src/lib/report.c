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

static void create_hash(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *pInput)
{
    unsigned char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, pInput, strlen(pInput));
    sha1_end(&sha1ctx, hash_bytes);

    unsigned len = SHA1_RESULT_LEN;
    unsigned char *s = hash_bytes;
    char *d = hash_str;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';
    //log("hash:%s str:'%s'", hash_str, pInput);
}

static void generate_hash_for_all(gpointer key, gpointer value, gpointer user_data)
{
    problem_item *pi = (problem_item *)value;
    char *hash_str = (char *)user_data;
    create_hash(hash_str, pi->content);
}

static int run_reporter_ui(char **args, int flags)
{
    char path[PATH_MAX+1];
    /*
    if is isatty
      -> run cli reporter
      path = "cli"
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
        strncpy(path, BIN_DIR"/bug-reporting-wizard", PATH_MAX);
        path[PATH_MAX] = 0;
        VERB1 log("Executing: %s", path);
        execv(path, args);
        /* Did not find abrt-gui in installation directory. Oh well */
        /* Trying to find it in PATH */
        strncpy(path, "bug-reporting-wizard", PATH_MAX);
        execvp(path, args);
        perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
    }
    else if(pid > 0)
    {
        if (flags & WAIT)
        {
            int status = 0;
            pid_t p = waitpid(pid, &status, WUNTRACED);
            if(p == -1)
            {
                error_msg("can't waitpid");
                return EXIT_FAILURE;
            }
            if (WIFEXITED(status))
            {
                VERB2 log("reporting finished with exitcode: status=%d\n", WEXITSTATUS(status));
                return WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                VERB2 log("reporting killed by signal %d\n", WTERMSIG(status));
            }
            else if (WIFSTOPPED(status))
            {
                /* should parent continue when the reporting is stopped??*/
                VERB2 log("reporting stopped by signal %d\n", WSTOPSIG(status));
            }
            else if (WIFCONTINUED(status))
            {
                VERB2 log("continued\n");
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
    char *dir_name = strdup(dd->dd_dirname);
    dd_close(dd);
    VERB2 log("Temp problem dir: '%s'\n", dir_name);
    result = analyze_and_report_dir(dir_name, flags);

    /* if we wait for reporter to finish, we can try to clean the tmp dir */
    if (flags & WAIT)
    {
        dd = dd_opendir(dir_name, 0);
        if (dd)
        {
            if (dd_delete(dd) != 0)
            {
                error_msg("Can't remove tmp dir: %s", dd->dd_dirname);
            }
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
    run_reporter_ui(args, flags);
    return 0;
}

int report(problem_data_t *pd)
{
    /* create hash from all components, so we at least eliminate the exact same
     * reports
    */
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    g_hash_table_foreach(pd, &generate_hash_for_all, hash_str);
    add_to_problem_data(pd, FILENAME_DUPHASH, hash_str);

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
    report_dir(dir_name);
    free(dir_name);

    return 0;
}
