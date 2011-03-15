#include "abrtlib.h"

GHashTable *g_event_config_list;

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
    free(p->name);
    free(p->value);
    free(p->label);
    //free(p->description);
    //free(p->allowed_value);
    free(p);
}

void free_event_config(event_config_t *p)
{
    if (!p)
        return;
    free(p->screen_name);
    //free(p->title);
    //free(p->action);
    free(p->description);
    for (GList *opt = p->options; opt; opt = opt->next)
        free_event_option(opt->data);
    g_list_free(p->options);
    free(p);
}


static int cmp_event_option_name_with_string(gconstpointer a, gconstpointer b)
{
    return strcmp(((event_option_t *)a)->name, (char *)b);
}

event_option_t *get_event_option_from_list(const char *name, GList *options)
{
    GList *elem = g_list_find_custom(options, name, &cmp_event_option_name_with_string);
    if (elem)
        return (event_option_t *)elem->data;
    return NULL;
}

/* (Re)loads data from /etc/abrt/events/foo.{xml,conf} */
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

    /* Load .conf files */
    dir = opendir(EVENTS_DIR);
    if (!dir)
        return;
    while ((dent = readdir(dir)) != NULL)
    {
        char *ext = strrchr(dent->d_name, '.');
        if (!ext)
            continue;
        if (strcmp(ext + 1, "conf") != 0)
            continue;

        char *fullname = concat_path_file(EVENTS_DIR, dent->d_name);

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
            GList *elem = g_list_find_custom(event_config->options, name, &cmp_event_option_name_with_string);
            if (elem)
            {
                opt = elem->data;
                //log("conf: replacing '%s' value:'%s'->'%s'", name, opt->value, value);
                free(opt->value);
            }
            else
            {
                //log("conf: new value %s='%s'", name, value);
                opt = new_event_option();
                opt->name = xstrdup(name);
            }
            opt->value = xstrdup(value);
            if (!elem)
                event_config->options = g_list_append(event_config->options, opt);
        }

        free_map_string(keys_and_values);

        if (new_config)
            g_hash_table_replace(g_event_config_list, xstrdup(dent->d_name), event_config);
    }
    closedir(dir);
}

/* Frees all loaded data */
void free_event_config_data(void)
{
    if (g_event_config_list)
    {
        g_hash_table_destroy(g_event_config_list);
        g_event_config_list = NULL;
    }
}

event_config_t *get_event_config(const char *name)
{
    if (!g_event_config_list)
        return NULL;
    return g_hash_table_lookup(g_event_config_list, name);
}
