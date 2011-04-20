/*
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
#include <fnmatch.h>
#include "abrtlib.h"
#include "rpm.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-save-package-data"

// TODO: convert g_settings_foo usage to command-line switches

/**
 * Returns the first full path argument in the command line or NULL.
 * Skips options (params of the form "-XXX").
 * Returns malloc'ed string.
 * NB: the cmdline is delimited by (single, not multiple) spaces, not tabs!
 * "abc def\t123" means there are two params: "abc", "def\t123".
 * "abc  def" means there are three params: "abc", "", "def".
 */
static char *get_argv1_if_full_path(const char* cmdline)
{
    const char *argv1 = strchr(cmdline, ' ');
    while (argv1 != NULL)
    {
        /* we found space in cmdline, so it might contain
         * path to some script like:
         * /usr/bin/python [-XXX] /usr/bin/system-control-network
         */
        argv1++; /* skip the space */
        if (*argv1 != '-')
            break;
        /* looks like -XXX in "perl -XXX /usr/bin/script.pl", skipping */
        argv1 = strchr(argv1, ' ');
    }

    /* if the string following the space doesn't start
     * with '/', it is not a full path to script
     * and we can't use it to determine the package name
     */
    if (*argv1 != '/')
        return NULL;

    /* good, it has "/foo/bar" form, return it */
    int len = strchrnul(argv1, ' ') - argv1;
    return xstrndup(argv1, len);
}

static bool is_path_blacklisted(const char *path)
{
    GList *li;
    for (li = g_settings_setBlackListedPaths; li != NULL; li = g_list_next(li))
    {
        if (fnmatch((char*)li->data, path, /*flags:*/ 0) == 0)
        {
            return true;
        }
    }
    return false;
}

static int SavePackageDescriptionToDebugDump(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *cmdline = NULL;
    char *executable = NULL;
    char *script_name = NULL; /* only if "interpreter /path/to/script" */
    char *package_short_name = NULL;
    char *package_full_name = NULL;
    char *component = NULL;
    int error = 1;
    /* note: "goto ret" statements below free all the above variables,
     * but they don't dd_close(dd) */

    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    bool kernel = (strcmp(analyzer, "Kerneloops") == 0);
    free(analyzer);
    if (kernel)
    {
        package_full_name = xstrdup("kernel");
        component = xstrdup("kernel");
    }
    else
    {
        char *remote_str = dd_load_text_ext(dd, FILENAME_REMOTE, DD_FAIL_QUIETLY_ENOENT);
        bool remote = string_to_bool(remote_str);
        free(remote_str);

        cmdline = dd_load_text(dd, FILENAME_CMDLINE);
        executable = dd_load_text(dd, FILENAME_EXECUTABLE);

        /* Close dd while we query package database. It can take some time,
         * don't want to keep dd locked longer than necessary */
        dd_close(dd);

        if (is_path_blacklisted(executable))
        {
            log("Blacklisted executable '%s'", executable);
            goto ret; /* return 1 (failure) */
        }

        package_full_name = rpm_get_package_nvr(executable);
        if (!package_full_name)
        {
            if (g_settings_bProcessUnpackaged || remote)
            {
                VERB2 log("Crash in unpackaged executable '%s', proceeding without packaging information", executable);
                dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
                if (!dd)
                    goto ret; /* return 1 (failure) */
                dd_save_text(dd, FILENAME_PACKAGE, "");
                dd_save_text(dd, FILENAME_COMPONENT, "");
                goto ret0; /* no error */
            }
            log("Executable '%s' doesn't belong to any package", executable);
            goto ret; /* return 1 (failure) */
        }

        /* Check well-known interpreter names */
        {
            const char *basename = strrchr(executable, '/');
            if (basename) basename++; else basename = executable;

            /* Add more interpreters as needed */
            if (strcmp(basename, "python") == 0
             || strcmp(basename, "perl") == 0
            ) {
// TODO: we don't verify that python executable is not modified
// or that python package is properly signed
// (see CheckFingerprint/CheckHash below)
                /* Try to find package for the script by looking at argv[1].
                 * This will work only if the cmdline contains the whole path.
                 * Example: python /usr/bin/system-control-network
                 */
                char *script_pkg = NULL;
                char *script_name = get_argv1_if_full_path(cmdline);
                if (script_name)
                {
                    script_pkg = rpm_get_package_nvr(script_name);
                    if (script_pkg)
                    {
                        /* There is a well-formed script name in argv[1],
                         * and it does belong to some package.
                         * Replace interpreter's package_full_name and executable
                         * with data pertaining to the script.
                         */
                        free(package_full_name);
                        package_full_name = script_pkg;
                        executable = script_name;
                        /* executable has changed, check it again */
                        if (is_path_blacklisted(executable))
                        {
                            log("Blacklisted executable '%s'", executable);
                            goto ret; /* return 1 (failure) */
                        }
                    }
                }
                if (!script_pkg && !g_settings_bProcessUnpackaged && !remote)
                {
                    log("Interpreter crashed, but no packaged script detected: '%s'", cmdline);
                    goto ret; /* return 1 (failure) */
                }
            }
        }

        package_short_name = get_package_name_from_NVR_or_NULL(package_full_name);
        VERB2 log("Package:'%s' short:'%s'", package_full_name, package_short_name);

        GList *li;

        for (li = g_settings_setBlackListedPkgs; li != NULL; li = g_list_next(li))
        {
            if (strcmp((char*)li->data, package_short_name) == 0)
            {
                log("Blacklisted package '%s'", package_short_name);
                goto ret; /* return 1 (failure) */
            }
        }

        if (g_settings_bOpenGPGCheck && !remote)
        {
            if (!rpm_chk_fingerprint(package_short_name))
            {
                log("Package '%s' isn't signed with proper key", package_short_name);
                goto ret; /* return 1 (failure) */
            }
            /* We used to also check the integrity of the executable here:
             *  if (!CheckHash(package_short_name.c_str(), executable)) BOOM();
             * Checking the MD5 sum requires to run prelink to "un-prelink" the
             * binaries - this is considered potential security risk so we don't
             * do it now, until we find some non-intrusive way.
             */
        }

        component = rpm_get_component(executable);

        dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
            goto ret; /* return 1 (failure) */
    }

    if (package_full_name)
    {
        dd_save_text(dd, FILENAME_PACKAGE, package_full_name);
    }
    if (component)
    {
        dd_save_text(dd, FILENAME_COMPONENT, component);
    }

    dd_close(dd);

 ret0:
    error = 0;
 ret:
    free(cmdline);
    free(executable);
    free(script_name);
    free(package_short_name);
    free(package_full_name);
    free(component);

    return error;
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -d DIR\n"
        "\n"
        "Query package database and save package name, component, and description"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    VERB1 log("Loading settings");
    if (load_abrt_conf() != 0)
        return 1; /* syntax error (logged already by load_abrt_conf) */

    VERB1 log("Initializing rpm library");
    rpm_init();

    GList *li;
    for (li = g_settings_setOpenGPGPublicKeys; li != NULL; li = g_list_next(li))
    {
        VERB1 log("Loading GPG key '%s'", (char*)li->data);
        rpm_load_gpgkey((char*)li->data);
    }

    return SavePackageDescriptionToDebugDump(dump_dir_name);
    /* can call rpm_destroy but do we really need to bother? we are exiting! */
}
