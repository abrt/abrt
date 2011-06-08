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

GHashTable *g_event_config_list;
static GHashTable *g_event_config_symlinks;

event_option_t *new_event_option(void)
{
    return xzalloc(sizeof(event_option_t));
}

event_config_t *new_event_config(void)
{
    return xzalloc(sizeof(event_config_t));
}

void free_event_option(event_option_t *p)
{
    if (!p)
        return;
    free(p->eo_name);
    free(p->eo_value);
    free(p->eo_label);
    free(p->eo_note_html);
    //free(p->eo_description);
    //free(p->eo_allowed_value);
    free(p);
}

void free_event_config(event_config_t *p)
{
    if (!p)
        return;

    free(p->screen_name);
    free(p->description);
    free(p->long_descr);
    free(p->ec_creates_items);
    free(p->ec_requires_items);
    free(p->ec_exclude_items_by_default);
    free(p->ec_include_items_by_default);
    free(p->ec_exclude_items_always);
    GList *opt;
    for (opt = p->options; opt; opt = opt->next)
        free_event_option(opt->data);
    g_list_free(p->options);

    free(p);
}


static int cmp_event_option_name_with_string(gconstpointer a, gconstpointer b)
{
    const event_option_t *evopt = a;
    return !evopt->eo_name || strcmp(evopt->eo_name, (char *)b) != 0;
}

event_option_t *get_event_option_from_list(const char *name, GList *options)
{
    GList *elem = g_list_find_custom(options, name, &cmp_event_option_name_with_string);
    if (elem)
        return (event_option_t *)elem->data;
    return NULL;
}

static void load_config_files(const char *dir_path)
{
    DIR *dir;
    struct dirent *dent;

    /* Load .conf files */
    dir = opendir(dir_path);
    if (!dir)
        return;
    while ((dent = readdir(dir)) != NULL)
    {
        char *ext = strrchr(dent->d_name, '.');
        if (!ext)
            continue;
        if (strcmp(ext + 1, "conf") != 0)
            continue;

        char *fullname = concat_path_file(dir_path, dent->d_name);

        *ext = '\0';
        event_config_t *event_config = get_event_config(dent->d_name);
        bool new_config = (!event_config);
        if (new_config)
            event_config = new_event_config();

        map_string_h *keys_and_values = new_map_string();

        load_conf_file(fullname, keys_and_values, /*skipKeysWithoutValue:*/ false);
        free(fullname);

        /* Insert or replace every key/value from keys_and_values to event_config->option */
        GHashTableIter iter;
        char *name;
        char *value;
        g_hash_table_iter_init(&iter, keys_and_values);
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
        {
            event_option_t *opt;
            GList *elem = g_list_find_custom(event_config->options, name,
                                            cmp_event_option_name_with_string);
            if (elem)
            {
                opt = elem->data;
                // log("conf: replacing '%s' value:'%s'->'%s'", name, opt->value, value);
                free(opt->eo_value);
            }
            else
            {
                // log("conf: new value %s='%s'", name, value);
                opt = new_event_option();
                opt->eo_name = xstrdup(name);
            }
            opt->eo_value = xstrdup(value);
            if (!elem)
                event_config->options = g_list_append(event_config->options, opt);
        }

        free_map_string(keys_and_values);

        if (new_config)
            g_hash_table_replace(g_event_config_list, xstrdup(dent->d_name), event_config);
    }
    closedir(dir);
}

/* (Re)loads data from /etc/abrt/events/foo.{xml,conf} and ~/.abrt/events/foo.conf */
void load_event_config_data(void)
{
    free_event_config_data();

    if (!g_event_config_list)
        g_event_config_list = g_hash_table_new_full(
                /*hash_func*/ g_str_hash,
                /*key_equal_func:*/ g_str_equal,
                /*key_destroy_func:*/ free,
                /*value_destroy_func:*/ (GDestroyNotify) free_event_config
        );
    if (!g_event_config_symlinks)
        g_event_config_symlinks = g_hash_table_new_full(
                /*hash_func*/ g_str_hash,
                /*key_equal_func:*/ g_str_equal,
                /*key_destroy_func:*/ free,
                /*value_destroy_func:*/ free
        );

    DIR *dir;
    struct dirent *dent;

    /* Load .xml files */
    dir = opendir(EVENTS_DIR);
    if (!dir)
        return;
    while ((dent = readdir(dir)) != NULL)
    {
        char *ext = strrchr(dent->d_name, '.');
        if (!ext)
            continue;
        if (strcmp(ext + 1, "xml") != 0)
            continue;

        char *fullname = concat_path_file(EVENTS_DIR, dent->d_name);
        *ext = '\0';

        struct stat buf;
        if (0 != lstat(fullname, &buf))
            continue;
        if (S_ISLNK(buf.st_mode))
        {
            GError *error = NULL;
            gchar *link = g_file_read_link(fullname, &error);
            if (error != NULL)
                error_msg_and_die("Error reading symlink '%s': %s", fullname, error->message);

            gchar *target = g_path_get_basename(link);
            char *ext = strrchr(target, '.');
            if (!ext || 0 != strcmp(ext + 1, "xml"))
                error_msg_and_die("Invalid event symlink '%s': expected it to"
                                  " point to another xml file", fullname);
            *ext = '\0';
            g_hash_table_replace(g_event_config_symlinks, xstrdup(dent->d_name), target);
            g_free(link);
            /* don't free target, it is owned by the hash table now */
            continue;
        }

        event_config_t *event_config = get_event_config(dent->d_name);
        bool new_config = (!event_config);
        if (new_config)
            event_config = new_event_config();

        load_event_description_from_file(event_config, fullname);
        free(fullname);

        if (new_config)
            g_hash_table_replace(g_event_config_list, xstrdup(dent->d_name), event_config);
    }
    closedir(dir);

    load_config_files(EVENTS_DIR);

    char *HOME = getenv("HOME");
    if (!HOME || !HOME[0])
        return;
    HOME = concat_path_file(HOME, ".abrt/events");
    load_config_files(HOME);
    free(HOME);
}

/* Frees all loaded data */
void free_event_config_data(void)
{
    if (g_event_config_list)
    {
        g_hash_table_destroy(g_event_config_list);
        g_event_config_list = NULL;
    }
    if (g_event_config_symlinks)
    {
        g_hash_table_destroy(g_event_config_symlinks);
        g_event_config_symlinks = NULL;
    }
}

event_config_t *get_event_config(const char *name)
{
    if (!g_event_config_list)
        return NULL;
    if (g_event_config_symlinks)
    {
        char *link = g_hash_table_lookup(g_event_config_symlinks, name);
        if (link)
            name = link;
    }
    return g_hash_table_lookup(g_event_config_list, name);
}

GList *export_event_config(const char *event_name)
{
    GList *env_list = NULL;

    event_config_t *config = get_event_config(event_name);
    if (config)
    {
        GList *lopt;
        for (lopt = config->options; lopt; lopt = lopt->next)
        {
            event_option_t *opt = lopt->data;
            if (!opt->eo_value)
                continue;
            char *var_val = xasprintf("%s=%s", opt->eo_name, opt->eo_value);
            VERB3 log("Exporting '%s'", var_val);
            env_list = g_list_prepend(env_list, var_val);
            putenv(var_val);
        }
    }

    return env_list;
}

void unexport_event_config(GList *env_list)
{
    while (env_list)
    {
        char *var_val = env_list->data;
        VERB3 log("Unexporting '%s'", var_val);
        safe_unsetenv(var_val);
        env_list = g_list_remove(env_list, var_val);
        free(var_val);
    }
}

/* return NULL if successful otherwise appropriate error message */
static char *validate_event_option(event_option_t *opt)
{
    if (!opt->eo_allow_empty && (!opt->eo_value || !opt->eo_value[0]))
        return xstrdup(_("Missing mandatory value"));

    /* if value is NULL and allow-empty yes than it doesn't make sence to check it */
    if (!opt->eo_value)
        return NULL;

    const gchar *s = NULL;
    if (!g_utf8_validate(opt->eo_value, -1, &s))
            return xasprintf(_("Invalid utf8 character '%c'"), *s);

    switch (opt->eo_type) {
    case OPTION_TYPE_TEXT:
    case OPTION_TYPE_PASSWORD:
        break;
    case OPTION_TYPE_NUMBER:
    {
        char *endptr;
        errno = 0;
        long r = strtol(opt->eo_value, &endptr, 10);
        (void) r;
        if (errno != 0 || endptr == opt->eo_value || *endptr != '\0')
            return xasprintf(_("Invalid number '%s'"), opt->eo_value);

        break;
    }
    case OPTION_TYPE_BOOL:
        if (strcmp(opt->eo_value, "yes") != 0
            && strcmp(opt->eo_value, "no") != 0
            && strcmp(opt->eo_value, "on") != 0
            && strcmp(opt->eo_value, "off") != 0
            && strcmp(opt->eo_value, "1") != 0
            && strcmp(opt->eo_value, "0") != 0)
        {
            return xasprintf(_("Invalid boolean value '%s'"), opt->eo_value);
        }
        break;
    case OPTION_TYPE_HINT_HTML:
        return NULL;
    default:
        return xstrdup(_("Unsupported option type"));
    };

    return NULL;
}

GHashTable *validate_event(const char *event_name)
{
    event_config_t *config = get_event_config(event_name);
    if (!config)
        return NULL;

    GHashTable *errors = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
    GList *li;

    for (li = config->options; li; li = li->next)
    {
        event_option_t *opt = (event_option_t *)li->data;
        char *err = validate_event_option(opt);
        if (err)
            g_hash_table_insert(errors, xstrdup(opt->eo_name), err);
    }

    if (g_hash_table_size(errors))
        return errors;

    g_hash_table_destroy(errors);

    return NULL;
}
