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
#include "libabrt.h"
#include "rpm.h"

#define GPG_CONF "gpg_keys.conf"

static bool   settings_bOpenGPGCheck = false;
static GList *settings_setOpenGPGPublicKeys = NULL;
static GList *settings_setBlackListedPkgs = NULL;
static GList *settings_setBlackListedPaths = NULL;
static bool   settings_bProcessUnpackaged = false;
static GList *settings_Interpreters = NULL;

static void ParseCommon(map_string_t *settings, const char *conf_filename)
{
    const char *value;

    value = get_map_string_item_or_NULL(settings, "OpenGPGCheck");
    if (value)
    {
        settings_bOpenGPGCheck = string_to_bool(value);
        remove_map_string_item(settings, "OpenGPGCheck");
    }

    value = get_map_string_item_or_NULL(settings, "BlackList");
    if (value)
    {
        settings_setBlackListedPkgs = parse_list(value);
        remove_map_string_item(settings, "BlackList");
    }

    value = get_map_string_item_or_NULL(settings, "BlackListedPaths");
    if (value)
    {
        settings_setBlackListedPaths = parse_list(value);
        remove_map_string_item(settings, "BlackListedPaths");
    }

    value = get_map_string_item_or_NULL(settings, "ProcessUnpackaged");
    if (value)
    {
        settings_bProcessUnpackaged = string_to_bool(value);
        remove_map_string_item(settings, "ProcessUnpackaged");
    }

    value = get_map_string_item_or_NULL(settings, "Interpreters");
    if (value)
    {
        settings_Interpreters = parse_list(value);
        remove_map_string_item(settings, "Interpreters");
    }

    map_string_iter_t iter;
    const char *name;
    /*char *value; - already declared */
    init_map_string_iter(&iter, settings);
    while (next_map_string_iter(&iter, &name, &value))
    {
        error_msg("Unrecognized variable '%s' in '%s'", name, conf_filename);
    }
}

static void load_gpg_keys(void)
{
    map_string_t *settings = new_map_string();
    if (!load_abrt_conf_file(GPG_CONF, settings))
    {
        error_msg("Can't load '%s'", GPG_CONF);
        return;
    }

    const char *gpg_keys_dir = get_map_string_item_or_NULL(settings, "GPGKeysDir");
    if (gpg_keys_dir != NULL && strcmp(gpg_keys_dir, "") != 0)
    {
        log_debug("Reading gpg keys from '%s'", gpg_keys_dir);
        GHashTable *done_set = g_hash_table_new(g_str_hash, g_str_equal);
        GList *gpg_files = get_file_list(gpg_keys_dir, NULL /* we don't care about the file ext */);
        for (GList *iter = gpg_files; iter; iter = g_list_next(iter))
        {
            const char *key_path = fo_get_fullpath((file_obj_t *)iter->data);

            if (g_hash_table_contains(done_set, key_path))
                continue;

            g_hash_table_insert(done_set, (gpointer)key_path, NULL);
            log_debug("Loading gpg key '%s'", key_path);
            settings_setOpenGPGPublicKeys = g_list_append(settings_setOpenGPGPublicKeys, xstrdup(key_path));
        }

        g_list_free_full(gpg_files, (GDestroyNotify)free_file_obj);
        g_hash_table_destroy(done_set);
    }
}

static int load_conf(const char *conf_filename)
{
    map_string_t *settings = new_map_string();
    if (conf_filename != NULL)
    {
        if (!load_conf_file(conf_filename, settings, false))
            error_msg("Can't open '%s'", conf_filename);
    }
    else
    {
        conf_filename = "abrt-action-save-package-data.conf";
        if (!load_abrt_conf_file(conf_filename, settings))
            error_msg("Can't load '%s'", conf_filename);
    }

    ParseCommon(settings, conf_filename);
    free_map_string(settings);

    load_gpg_keys();

    return 0;
}

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
    if (argv1 == NULL || *argv1 != '/')
        return NULL;

    /* good, it has "/foo/bar" form, return it */
    int len = strchrnul(argv1, ' ') - argv1;
    return xstrndup(argv1, len);
}

static bool is_path_blacklisted(const char *path)
{
    GList *li;
    for (li = settings_setBlackListedPaths; li != NULL; li = g_list_next(li))
    {
        if (fnmatch((char*)li->data, path, /*flags:*/ 0) == 0)
        {
            return true;
        }
    }
    return false;
}

static struct pkg_envra *get_script_name(const char *cmdline, char **executable, const char *chroot)
{
// TODO: we don't verify that python executable is not modified
// or that python package is properly signed
// (see CheckFingerprint/CheckHash below)
    /* Try to find package for the script by looking at argv[1].
     * This will work only if the cmdline contains the whole path.
     * Example: python /usr/bin/system-control-network
     */
    struct pkg_envra *script_pkg = NULL;
    char *script_name = get_argv1_if_full_path(cmdline);
    if (script_name)
    {
        script_pkg = rpm_get_package_nvr(script_name, chroot);
        if (script_pkg)
        {
            /* There is a well-formed script name in argv[1],
             * and it does belong to some package.
             * Replace executable
             * with data pertaining to the script.
             */
            *executable = script_name;
        }
    }

    return script_pkg;
}

static int SavePackageDescriptionToDebugDump(const char *dump_dir_name, const char *chroot)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *type = dd_load_text(dd, FILENAME_TYPE);
    if (!strcmp(type, "Kerneloops"))
    {
        dd_save_text(dd, FILENAME_PACKAGE, "kernel");
        dd_save_text(dd, FILENAME_COMPONENT, "kernel");
        dd_close(dd);
        free(type);
        return 0;
    }
    free(type);

    char *cmdline = NULL;
    char *executable = NULL;
    char *rootdir = NULL;
    char *package_short_name = NULL;
    struct pkg_envra *pkg_name = NULL;
    char *component = NULL;
    int error = 1;
    /* note: "goto ret" statements below free all the above variables,
     * but they don't dd_close(dd) */

    cmdline = dd_load_text_ext(dd, FILENAME_CMDLINE, DD_FAIL_QUIETLY_ENOENT);
    executable = dd_load_text(dd, FILENAME_EXECUTABLE);

    /* Do not implicitly query rpm database in process's root dir, if
     * ExploreChroots is disabled. */
    if (g_settings_explorechroots && chroot == NULL)
        chroot = rootdir = dd_load_text_ext(dd, FILENAME_ROOTDIR,
                               DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);

    /* Close dd while we query package database. It can take some time,
     * don't want to keep dd locked longer than necessary */
    dd_close(dd);

    if (is_path_blacklisted(executable))
    {
        log("Blacklisted executable '%s'", executable);
        goto ret; /* return 1 (failure) */
    }

    pkg_name = rpm_get_package_nvr(executable, chroot);
    if (!pkg_name)
    {
        if (settings_bProcessUnpackaged)
        {
            log_info("Crash in unpackaged executable '%s', "
                      "proceeding without packaging information", executable);
            goto ret0; /* no error */
        }
        log("Executable '%s' doesn't belong to any package"
		" and ProcessUnpackaged is set to 'no'",
		executable
        );
        goto ret; /* return 1 (failure) */
    }

    /* Check well-known interpreter names */
    const char *basename = strrchr(executable, '/');
    if (basename)
        basename++;
    else
        basename = executable;

    /* if basename is known interpreter, we want to blame the running script
     * not the interpreter
     */
    if (g_list_find_custom(settings_Interpreters, basename, (GCompareFunc)g_strcmp0))
    {
        struct pkg_envra *script_pkg = get_script_name(cmdline, &executable, chroot);
        /* executable may have changed, check it again */
        if (is_path_blacklisted(executable))
        {
            log("Blacklisted executable '%s'", executable);
            goto ret; /* return 1 (failure) */
        }
        if (!script_pkg)
        {
            /* Script name is not absolute, or it doesn't
             * belong to any installed package.
             */
            if (!settings_bProcessUnpackaged)
            {
                log("Interpreter crashed, but no packaged script detected: '%s'", cmdline);
                goto ret; /* return 1 (failure) */
            }

            /* Unpackaged script, but the settings says we want to keep it.
             * BZ plugin wont allow to report this anyway, because component
             * is missing, so there is no reason to mark it as not_reportable.
             * Someone might want to use abrt to report it using ftp.
             */
            goto ret0;
        }

        free_pkg_envra(pkg_name);
        pkg_name = script_pkg;
    }

    package_short_name = xasprintf("%s", pkg_name->p_name);
    log_info("Package:'%s' short:'%s'", pkg_name->p_nvr, package_short_name);


    if (g_list_find_custom(settings_setBlackListedPkgs, package_short_name, (GCompareFunc)g_strcmp0))
    {
        log("Blacklisted package '%s'", package_short_name);
        goto ret; /* return 1 (failure) */
    }

    if (settings_bOpenGPGCheck)
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

    component = rpm_get_component(executable, chroot);

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        goto ret; /* return 1 (failure) */

    if (pkg_name)
    {
        dd_save_text(dd, FILENAME_PACKAGE, pkg_name->p_nvr);
        dd_save_text(dd, FILENAME_PKG_EPOCH, pkg_name->p_epoch);
        dd_save_text(dd, FILENAME_PKG_NAME, pkg_name->p_name);
        dd_save_text(dd, FILENAME_PKG_VERSION, pkg_name->p_version);
        dd_save_text(dd, FILENAME_PKG_RELEASE, pkg_name->p_release);
        dd_save_text(dd, FILENAME_PKG_ARCH, pkg_name->p_arch);
    }

    if (component)
        dd_save_text(dd, FILENAME_COMPONENT, component);

    dd_close(dd);

 ret0:
    error = 0;
 ret:
    free(cmdline);
    free(executable);
    free(rootdir);
    free(package_short_name);
    free_pkg_envra(pkg_name);
    free(component);

    return error;
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = ".";
    const char *conf_filename = NULL;
    const char *chroot = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [-c CONFFILE] [-r CHROOT] -d DIR\n"
        "\n"
        "Query package database and save package and component name"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_c = 1 << 2,
        OPT_r = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"     , _("Problem directory")),
        OPT_STRING('c', NULL, &conf_filename, "CONFFILE", _("Configuration file")),
        OPT_STRING('r', "chroot", &chroot,    "CHROOT"  , _("Use this directory as RPM root")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    log_notice("Loading settings");
    if (load_conf(conf_filename) != 0)
        return 1; /* syntax error (logged already by load_conf) */

    log_notice("Initializing rpm library");
    rpm_init();

    GList *li;
    for (li = settings_setOpenGPGPublicKeys; li != NULL; li = g_list_next(li))
    {
        log_notice("Loading GPG key '%s'", (char*)li->data);
        rpm_load_gpgkey((char*)li->data);
    }

    int r = SavePackageDescriptionToDebugDump(dump_dir_name, chroot);

    /* Close RPM database */
    rpm_destroy();

    return r;
}
