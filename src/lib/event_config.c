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
    free(p->description);
    free(p->allowed_value);
    free(p);
}

void free_event_config(event_config_t *p)
{
    if (!p)
        return;
    free(p->name);
    free(p->title);
    free(p->action);
    free(p->description);
    for (GList *opt = p->options; opt; opt = opt->next)
        free_event_option(opt->data);
    g_list_free(p->options);
    free(p);
}


// (Re)loads data from /etc/abrt/events/*.{conf,xml}
void load_event_config_data(void)
{
    free_event_config_data();
    DIR *dir = opendir(EVENTS_DIR);
    if (!dir)
        return;

    if (!g_event_config_list)
        g_event_config_list = g_hash_table_new_full(
                /*hash_func*/ g_str_hash,
                /*key_equal_func:*/ g_str_equal,
                /*key_destroy_func:*/ free,
                /*value_destroy_func:*/ (GDestroyNotify) free_event_config
        );

    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL)
    {
        char *ext = strrchr(dent->d_name, '.');
        if (!ext)
            continue;
        ext++;
        bool conf = strcmp(ext, "conf") == 0;
        bool xml = strcmp(ext, "xml") == 0;
        if (!conf && !xml)
            continue;

        event_config_t *event_config = new_event_config();

        char *fullname = concat_path_file(EVENTS_DIR, dent->d_name);
        if (xml)
            load_event_description_from_file(event_config, fullname);

//        if (conf)
//            load_event_values_from_file(event_config, fullname);

        free(fullname);

        //we did ext++ so we need ext-- to point to '.'
        *(--ext) = '\0';
        g_hash_table_replace(g_event_config_list, xstrdup(dent->d_name), event_config);
    }
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
