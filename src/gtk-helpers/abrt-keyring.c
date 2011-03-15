#include <gnome-keyring.h>
#include <string.h>
#include <stdlib.h>
#include "abrtlib.h"

static char *keyring;

static guint32 search_item_id(const char *event_name)
{
    GnomeKeyringAttributeList *attrs = gnome_keyring_attribute_list_new();
    GList *found;
    //let's hope 0 is not valid item_id
    guint32 item_id = 0;
    gnome_keyring_attribute_list_append_string(attrs, "libreportEventConfig", event_name);
    GnomeKeyringResult result = gnome_keyring_find_items_sync(
                                GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                attrs,
                                &found);
    if (result != GNOME_KEYRING_RESULT_OK)
        return item_id;
    if (found)
    {
        item_id = ((GnomeKeyringFound *)found->data)->item_id;
        gnome_keyring_found_list_free(found);
    }
    return item_id;
}

void abrt_keyring_save_settings(const char *event_name)
{
    GList *l;
    GnomeKeyringAttributeList *attrs = gnome_keyring_attribute_list_new();
    guint32 item_id;
    event_config_t *ec = get_event_config(event_name);
    /* add string id which we use to search for items */
    gnome_keyring_attribute_list_append_string(attrs, "libreportEventConfig", event_name);
    for (l = g_list_first(ec->options); l != NULL; l = g_list_next(l))
    {
        event_option_t *op = (event_option_t *)l->data;
        gnome_keyring_attribute_list_append_string(attrs, op->name, op->value);
    }

    GnomeKeyringResult result;
    item_id = search_item_id(event_name);
    if (item_id)
    {
        VERB2 log("updating item with id: %i", item_id);
        /* found existing item, so just update the values */
        result = gnome_keyring_item_set_attributes_sync(keyring, item_id, attrs);
    }
    else
    {
        /* did't find existing item, so create a new one */
        result = gnome_keyring_item_create_sync(keyring,
                                     GNOME_KEYRING_ITEM_GENERIC_SECRET, /* type */
                                     event_name, /* display name */
                                     attrs, /* attributes */
                                     NULL, /* secret - no special handling for password it's stored in attrs */
                                     1, /* update if exist */
                                     &item_id);
        VERB2 log("created new item with id: %i", item_id);
    }

    if (result != GNOME_KEYRING_RESULT_OK)
    {
        VERB2 log("error occured, settings is not saved!");
        return;
    }
    VERB2 log("saved");
}

static void abrt_keyring_load_settings(const char *event_name, event_config_t *ec)
{
    GnomeKeyringAttributeList *attrs = gnome_keyring_attribute_list_new();
    guint item_id = search_item_id(event_name);
    if (!item_id)
        return;
    GnomeKeyringResult result = gnome_keyring_item_get_attributes_sync(
                                    keyring,
                                    item_id,
                                    &attrs);
    VERB2 log("num attrs %i", attrs->len);
    if (result != GNOME_KEYRING_RESULT_OK)
        return;
    guint index;

    for (index = 0; index < attrs->len; index++)
    {
        char *name = g_array_index(attrs, GnomeKeyringAttribute, index).name;
VERB2 log("load %s", name);
        event_option_t *option = get_event_option_from_list(name, ec->options);
        if (option)
            option->value = g_array_index(attrs, GnomeKeyringAttribute, index).value.string;
VERB2 log("loaded %s", name);
        //VERB2 log("load %s", g_array_index(attrs, GnomeKeyringAttribute, index).value);

    }
}

static void init_keyring()
{
    //called again?
    if (keyring)
        return;
    if (!gnome_keyring_is_available())
    {
        VERB2 log("Cannot connect to the Gnome Keyring daemon.");
        return;
    }
    GnomeKeyringResult result = gnome_keyring_get_default_keyring_sync(&keyring);
    if (result != GNOME_KEYRING_RESULT_OK || keyring == NULL)
        VERB2 log("can't get the default kerying");
    /*
    The default keyring might not be set - in that case result = OK, but the
    keyring = NULL
    use gnome_keyring_list_keyring_names () to list all and pick the first one?
    */
    VERB2 log("%s", keyring);
}

void load_event_config(gpointer key, gpointer value, gpointer user_data)
{
    char* event_name = (char*)key;
    event_config_t *ec = (event_config_t *)value;
VERB2 log("from keyring loading: %s", event_name);
    abrt_keyring_load_settings(event_name, ec);

}

/*
 * Tries to load settings for all events in g_event_config_list
*/
void load_event_config_data_from_keyring()
{
    init_keyring();
    if (!keyring)
        return;
    g_hash_table_foreach(g_event_config_list, &load_event_config, NULL);
}
