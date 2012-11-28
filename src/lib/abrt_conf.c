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

char *        g_settings_sWatchCrashdumpArchiveDir = NULL;
unsigned int  g_settings_nMaxCrashReportsSize = 1000;
char *        g_settings_dump_location = NULL;
bool          g_settings_delete_uploaded = 0;
bool          g_settings_autoreporting = 0;
char *        g_settings_autoreporting_event = NULL;

void free_abrt_conf_data()
{
    free(g_settings_sWatchCrashdumpArchiveDir);
    g_settings_sWatchCrashdumpArchiveDir = NULL;

    free(g_settings_dump_location);
    g_settings_dump_location = NULL;
}

static void ParseCommon(map_string_h *settings, const char *conf_filename)
{
    char *value;

    value = g_hash_table_lookup(settings, "WatchCrashdumpArchiveDir");
    if (value)
    {
        g_settings_sWatchCrashdumpArchiveDir = xstrdup(value);
        g_hash_table_remove(settings, "WatchCrashdumpArchiveDir");
    }

    value = g_hash_table_lookup(settings, "MaxCrashReportsSize");
    if (value)
    {
        char *end;
        errno = 0;
        unsigned long ul = strtoul(value, &end, 10);
        if (errno || end == value || *end != '\0' || ul > INT_MAX)
            error_msg("Error parsing %s setting: '%s'", "MaxCrashReportsSize", value);
        else
            g_settings_nMaxCrashReportsSize = ul;
        g_hash_table_remove(settings, "MaxCrashReportsSize");
    }

    value = g_hash_table_lookup(settings, "DumpLocation");
    if (value)
    {
        g_settings_dump_location = xstrdup(value);
        g_hash_table_remove(settings, "DumpLocation");
    }
    else
        g_settings_dump_location = xstrdup("/var/spool/abrt");

    value = g_hash_table_lookup(settings, "DeleteUploaded");
    if (value)
    {
        g_settings_delete_uploaded = string_to_bool(value);
        g_hash_table_remove(settings, "DeleteUploaded");
    }

    value = g_hash_table_lookup(settings, "AutoreportingEnabled");
    if (value)
    {
        g_settings_autoreporting = string_to_bool(value);
        g_hash_table_remove(settings, "AutoreportingEnabled");
    }

    value = g_hash_table_lookup(settings, "AutoreportingEvent");
    if (value)
    {
        g_settings_autoreporting_event = xstrdup(value);
        g_hash_table_remove(settings, "AutoreportingEvent");
    }
    else
        g_settings_autoreporting_event = xstrdup("report_uReport");

    GHashTableIter iter;
    char *name;
    /*char *value; - already declared */
    g_hash_table_iter_init(&iter, settings);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        error_msg("Unrecognized variable '%s' in '%s'", name, conf_filename);
    }
}

int load_abrt_conf()
{
    free_abrt_conf_data();

    map_string_h *settings = new_map_string();
    if (!load_conf_file(CONF_DIR"/abrt.conf", settings, /*skip key w/o values:*/ false))
        perror_msg("Can't open '%s'", CONF_DIR"/abrt.conf");

    ParseCommon(settings, CONF_DIR"/abrt.conf");
    free_map_string(settings);

    return 0;
}
