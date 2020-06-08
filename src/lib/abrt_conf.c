/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

#define ABRT_CONF "abrt.conf"

char *        abrt_g_settings_sWatchCrashdumpArchiveDir = NULL;
unsigned int  abrt_g_settings_nMaxCrashReportsSize = 5000;
char *        abrt_g_settings_dump_location = NULL;
bool          abrt_g_settings_delete_uploaded = 0;
bool          abrt_g_settings_autoreporting = 0;
char *        abrt_g_settings_autoreporting_event = NULL;
bool          abrt_g_settings_shortenedreporting = 0;
bool          abrt_g_settings_explorechroots = 0;
unsigned int  abrt_g_settings_debug_level = 0;

void abrt_free_abrt_conf_data()
{
    free(abrt_g_settings_sWatchCrashdumpArchiveDir);
    abrt_g_settings_sWatchCrashdumpArchiveDir = NULL;

    free(abrt_g_settings_dump_location);
    abrt_g_settings_dump_location = NULL;

    free(abrt_g_settings_autoreporting_event);
    abrt_g_settings_autoreporting_event = NULL;
}

/* Beware - the function normalizes only slashes - that's the most often
 * problem we have to face.
 */
static char *xstrdup_normalized_path(const char *path)
{
    const size_t len = strlen(path);
    char *const res = g_malloc0(len + 1);

    res[0] = path[0];

    const char *p = path + 1;
    char *r = res;
    for (; p - path < len; ++p)
        if (*p != '/' || *r != '/')
            *++r = *p;

    /* remove trailing slash if the path is not '/' */
    if (r - res > 1 && *r == '/')
        *r = '\0';

    return res;
}

static void ParseCommon(GHashTable *settings, const char *conf_filename)
{
    gpointer value;

    value = g_hash_table_lookup(settings, "WatchCrashdumpArchiveDir");
    if (value)
    {
        abrt_g_settings_sWatchCrashdumpArchiveDir = xstrdup_normalized_path(value);
        g_hash_table_remove(settings, "WatchCrashdumpArchiveDir");
    }

    value = g_hash_table_lookup(settings, "MaxCrashReportsSize");
    if (value)
    {
        char *end;
        errno = 0;
        unsigned long ul = strtoul((char *)value, &end, 10);
        if (errno || end == value || *end != '\0' || ul > INT_MAX)
            error_msg("Error parsing %s setting: '%s'", "MaxCrashReportsSize", (char *)value);
        else
            abrt_g_settings_nMaxCrashReportsSize = ul;
        g_hash_table_remove(settings, "MaxCrashReportsSize");
    }

    value = g_hash_table_lookup(settings, "DumpLocation");
    if (value)
    {
        abrt_g_settings_dump_location = xstrdup_normalized_path((char *)value);
        g_hash_table_remove(settings, "DumpLocation");
    }
    else
        abrt_g_settings_dump_location = g_strdup(DEFAULT_DUMP_LOCATION);

    value = g_hash_table_lookup(settings, "DeleteUploaded");
    if (value)
    {
        abrt_g_settings_delete_uploaded = libreport_string_to_bool((char *)value);
        g_hash_table_remove(settings, "DeleteUploaded");
    }

    value = g_hash_table_lookup(settings, "AutoreportingEnabled");
    if (value)
    {
        abrt_g_settings_autoreporting = libreport_string_to_bool((char *)value);
        g_hash_table_remove(settings, "AutoreportingEnabled");
    }

    value = g_hash_table_lookup(settings, "AutoreportingEvent");
    if (value)
    {
        abrt_g_settings_autoreporting_event = g_strdup(value);
        g_hash_table_remove(settings, "AutoreportingEvent");
    }
    else
        abrt_g_settings_autoreporting_event = g_strdup("report_uReport");

    value = g_hash_table_lookup(settings, "ShortenedReporting");
    if (value)
    {
        abrt_g_settings_shortenedreporting = libreport_string_to_bool((char *)value);
        g_hash_table_remove(settings, "ShortenedReporting");
    }
    else
    {
        /* Default: enabled for GNOME desktop, else disabled */
        const char *desktop_env = getenv("DESKTOP_SESSION");
        abrt_g_settings_shortenedreporting = (desktop_env && strcasestr(desktop_env, "gnome") != NULL);
    }

    value = g_hash_table_lookup(settings, "ExploreChroots");
    if (value)
    {
        abrt_g_settings_explorechroots = libreport_string_to_bool((char *)value);
        g_hash_table_remove(settings, "ExploreChroots");
    }
    else
        abrt_g_settings_explorechroots = false;

    value = g_hash_table_lookup(settings, "DebugLevel");
    if (value)
    {
        char *end;
        errno = 0;
        unsigned long ul = strtoul((char *)value, &end, 10);
        if (errno || end == value || *end != '\0' || ul > INT_MAX)
            error_msg("Error parsing %s setting: '%s'", "DebugLevel", (char *)value);
        else
            abrt_g_settings_debug_level = ul;
        g_hash_table_remove(settings, "DebugLevel");
    }

    GHashTableIter iter;
    gpointer name;
    g_hash_table_iter_init(&iter, settings);
    while (g_hash_table_iter_next(&iter, &name, NULL))
    {
        error_msg("Unrecognized variable '%s' in '%s'", (char *)name, conf_filename);
    }
}

static const char *get_abrt_conf_file_name(void)
{
    const char *const abrt_conf = getenv("ABRT_CONF_FILE_NAME");
    return abrt_conf == NULL ? ABRT_CONF : abrt_conf;
}

int abrt_load_abrt_conf()
{
    abrt_free_abrt_conf_data();

    const char *const abrt_conf = get_abrt_conf_file_name();
    g_autoptr(GHashTable) settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (!abrt_load_abrt_conf_file(abrt_conf, settings))
        perror_msg("Can't load '%s'", abrt_conf);

    ParseCommon(settings, abrt_conf);

    return 0;
}

int abrt_load_abrt_conf_file(const char *file, GHashTable *settings)
{
    const char *env_conf_dir = getenv("ABRT_CONF_DIR");
    const char *const conf_directories[] = {
        env_conf_dir ? env_conf_dir : CONF_DIR,
        NULL
    };

    return libreport_load_conf_file_from_dirs(file, conf_directories, settings, /*skip key w/o values:*/ false);
}

int abrt_load_abrt_plugin_conf_file(const char *file, GHashTable *settings)
{
    static const char *const conf_directories[] = { PLUGINS_CONF_DIR, NULL };

    return libreport_load_conf_file_from_dirs(file, conf_directories, settings, /*skip key w/o values:*/ false);
}

int abrt_save_abrt_conf_file(const char *file, GHashTable *settings)
{
    g_autofree char *path = g_build_filename(CONF_DIR ? CONF_DIR : "", file, NULL);
    int retval = libreport_save_conf_file(path, settings);
    return retval;
}

int abrt_save_abrt_plugin_conf_file(const char *file, GHashTable *settings)
{
    g_autofree char *path = g_build_filename(PLUGINS_CONF_DIR ? PLUGINS_CONF_DIR : "", file, NULL);
    int retval = libreport_save_conf_file(path, settings);
    return retval;
}
