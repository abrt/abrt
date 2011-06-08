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
#include <gnome-keyring.h>
#include "libreport.h"

static char *keyring_name;
static bool got_keyring = 0;

static guint32 search_item_id(const char *event_name)
{
    GnomeKeyringAttributeList *attrs = gnome_keyring_attribute_list_new();
    GList *found = NULL;
    gnome_keyring_attribute_list_append_string(attrs, "libreportEventConfig", event_name);
    GnomeKeyringResult result = gnome_keyring_find_items_sync(
                                GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                attrs,
                                &found);
    gnome_keyring_attribute_list_free(attrs);

    //let's hope 0 is not valid item_id
    guint32 item_id = 0;
    if (result != GNOME_KEYRING_RESULT_OK)
        goto ret;
    if (found)
        item_id = ((GnomeKeyringFound *)found->data)->item_id;
 ret:
    if (found)
        gnome_keyring_found_list_free(found);
    VERB2 log("keyring has %sconfiguration for event '%s'", (item_id != 0) ? "" : "no ", event_name);
    return item_id;
}

void abrt_keyring_save_settings(const char *event_name)
{
    GList *l;
    GnomeKeyringAttributeList *attrs = gnome_keyring_attribute_list_new();
    event_config_t *ec = get_event_config(event_name);
    /* add string id which we use to search for items */
    gnome_keyring_attribute_list_append_string(attrs, "libreportEventConfig", event_name);
    for (l = g_list_first(ec->options); l != NULL; l = g_list_next(l))
    {
        event_option_t *op = (event_option_t *)l->data;
        gnome_keyring_attribute_list_append_string(attrs, op->eo_name, op->eo_value);
    }

    GnomeKeyringResult result;
    guint32 item_id = search_item_id(event_name);
    if (item_id)
    {
        /* found existing item, so just update the values */
        result = gnome_keyring_item_set_attributes_sync(keyring_name, item_id, attrs);
        VERB2 log("updated item with id: %i", item_id);
    }
    else
    {
        /* did't find existing item, so create a new one */
        result = gnome_keyring_item_create_sync(keyring_name,
                                     GNOME_KEYRING_ITEM_GENERIC_SECRET, /* type */
                                     event_name, /* display name */
                                     attrs, /* attributes */
                                     NULL, /* secret - no special handling for password it's stored in attrs */
                                     1, /* update if exist */
                                     &item_id);
        VERB2 log("created new item with id: %i", item_id);
    }
    gnome_keyring_attribute_list_free(attrs);

    if (result != GNOME_KEYRING_RESULT_OK)
    {
        error_msg("Error saving event '%s' configuration to keyring", event_name);
        return;
    }
    VERB2 log("saved event '%s' configuration to keyring", event_name);
}

static void abrt_keyring_load_settings(const char *event_name, event_config_t *ec)
{
    guint item_id = search_item_id(event_name);
    if (!item_id)
        return;
    GnomeKeyringAttributeList *attrs = NULL;
    GnomeKeyringResult result = gnome_keyring_item_get_attributes_sync(
                                    keyring_name,
                                    item_id,
                                    &attrs);
    if (result == GNOME_KEYRING_RESULT_OK && attrs)
    {
        VERB3 log("num attrs %i", attrs->len);
        guint index;
        for (index = 0; index < attrs->len; index++)
        {
            char *name = g_array_index(attrs, GnomeKeyringAttribute, index).name;
            VERB2 log("keyring has name '%s'", name);
            event_option_t *option = get_event_option_from_list(name, ec->options);
            if (option)
            {
                free(option->eo_value);
                option->eo_value = xstrdup(g_array_index(attrs, GnomeKeyringAttribute, index).value.string);
                VERB2 log("added or replaced in event config:'%s=%s'", name, option->eo_value);
            }
        }
    }
    if (attrs)
        gnome_keyring_attribute_list_free(attrs);
}

static void init_keyring()
{
    /* Called again? */
    if (got_keyring)
        return;

    if (!gnome_keyring_is_available())
    {
        error_msg("Cannot connect to Gnome keyring daemon");
        return;
    }

    GnomeKeyringResult result = gnome_keyring_get_default_keyring_sync(&keyring_name);
    if (result != GNOME_KEYRING_RESULT_OK)
    {
        error_msg("Can't get default keyring (result:%d)", result);
        return;
    }

    got_keyring = 1;
    /*
     * Note: The default keyring name can be NULL. It is a valid name.
     */
    VERB2 log("keyring:'%s'", keyring_name);
}

static void load_event_config(gpointer key, gpointer value, gpointer user_data)
{
    char* event_name = (char*)key;
    event_config_t *ec = (event_config_t *)value;
    VERB2 log("loading event '%s' configuration from keyring", event_name);
    abrt_keyring_load_settings(event_name, ec);
}

/*
 * Tries to load settings for all events in g_event_config_list
*/
void load_event_config_data_from_keyring()
{
    init_keyring();
    if (!got_keyring)
        return;
    g_hash_table_foreach(g_event_config_list, &load_event_config, NULL);
}
