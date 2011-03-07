#include "event_config.h"

GHashTable *g_event_config_list;

// (Re)loads data from /etc/abrt/events/*.{conf,xml}
void load_event_config_data(void)
{
    /* for each xml file call load_event_description_from_file */
    /* for each conf file call load_even_options_value_from_file?
     * - we don't have this
     * - should re-use the event_config structure created when parsing xml - if exists
     * - or should this one be called first?
     */
}
/* Frees all loaded data */
void free_event_config_data(void)
{

}

event_config_t *get_event_config(const char *name)
{
    //could g_event_config_list be null?
    gpointer event_config = g_hash_table_lookup(g_event_config_list, name);
    return event_config;
}
