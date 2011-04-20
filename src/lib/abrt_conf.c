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
#include "abrtlib.h"

bool          g_settings_bOpenGPGCheck = false;
GList *       g_settings_setOpenGPGPublicKeys = NULL;
GList *       g_settings_setBlackListedPkgs = NULL;
GList *       g_settings_setBlackListedPaths = NULL;
char *        g_settings_sWatchCrashdumpArchiveDir = NULL;
unsigned int  g_settings_nMaxCrashReportsSize = 1000;
bool          g_settings_bProcessUnpackaged = false;


void free_abrt_conf_data()
{
    list_free_with_free(g_settings_setOpenGPGPublicKeys);
    g_settings_setOpenGPGPublicKeys = NULL;

    list_free_with_free(g_settings_setBlackListedPkgs);
    g_settings_setBlackListedPkgs = NULL;

    list_free_with_free(g_settings_setBlackListedPaths);
    g_settings_setBlackListedPaths = NULL;

    free(g_settings_sWatchCrashdumpArchiveDir);
    g_settings_sWatchCrashdumpArchiveDir = NULL;
}

static GList *parse_list(const char* list)
{
    struct strbuf *item = strbuf_new();
    GList *l = NULL;

    char *trim_item = NULL;

    for (unsigned ii = 0; list[ii]; ii++)
    {
        if (list[ii] == ',')
        {
            trim_item = strtrim(item->buf);
            l = g_list_append(l, xstrdup(trim_item));
            strbuf_clear(item);
        }
        else
            strbuf_append_char(item, list[ii]);
    }

    if (item->len > 0)
    {
        trim_item = strtrim(item->buf);
        l = g_list_append(l, xstrdup(trim_item));
    }

    strbuf_free(item);

    return l;
}

static void ParseCommon(map_string_h *settings, const char *conf_filename)
{
    char *value;

    value = g_hash_table_lookup(settings, "OpenGPGCheck");
    if (value)
    {
        g_settings_bOpenGPGCheck = string_to_bool(value);
        g_hash_table_remove(settings, "OpenGPGCheck");
    }

    value = g_hash_table_lookup(settings, "BlackList");
    if (value)
    {
        g_settings_setBlackListedPkgs = parse_list(value);
        g_hash_table_remove(settings, "BlackList");
    }

    value = g_hash_table_lookup(settings, "BlackListedPaths");
    if (value)
    {
        g_settings_setBlackListedPaths = parse_list(value);
        g_hash_table_remove(settings, "BlackListedPaths");
    }

    value = g_hash_table_lookup(settings, "WatchCrashdumpArchiveDir");
    if (value)
    {
        g_settings_sWatchCrashdumpArchiveDir = xstrdup(value);
        g_hash_table_remove(settings, "WatchCrashdumpArchiveDir");
    }

    value = g_hash_table_lookup(settings, "MaxCrashReportsSize");
    if (value)
    {
//FIXME: dont die
        g_settings_nMaxCrashReportsSize = xatoi_positive(value);
        g_hash_table_remove(settings, "MaxCrashReportsSize");
    }

    value = g_hash_table_lookup(settings, "ProcessUnpackaged");
    if (value)
    {
        g_settings_bProcessUnpackaged = string_to_bool(value);
        g_hash_table_remove(settings, "ProcessUnpackaged");
    }

    GHashTableIter iter;
    char *name;
    /*char *value; - already declared */
    g_hash_table_iter_init(&iter, settings);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        error_msg("Unrecognized variable '%s' in '%s'", name, conf_filename);
    }
}

static void LoadGPGKeys()
{
    FILE *fp = fopen(CONF_DIR"/gpg_keys", "r");
    if (fp)
    {
        /* every line is one key
         * FIXME: make it more robust, it doesn't handle comments
         */
        char *line;
        while ((line = xmalloc_fgetline(fp)) != NULL)
        {
            if (line[0] == '/') // probably the beginning of a path, so let's handle it as a key
                g_settings_setOpenGPGPublicKeys = g_list_append(g_settings_setOpenGPGPublicKeys, line);
            else
                free(line);
        }
        fclose(fp);
    }
}

int load_abrt_conf()
{
    free_abrt_conf_data();

    map_string_h *settings = new_map_string();
    if (!load_conf_file(CONF_DIR"/abrt.conf", settings, /*skip key w/o values:*/ false))
        error_msg("Can't open '%s'", CONF_DIR"/abrt.conf");

    ParseCommon(settings, CONF_DIR"/abrt.conf");
    free_map_string(settings);

    LoadGPGKeys();

    return 0;
}
