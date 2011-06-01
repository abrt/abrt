/*
    Copyright (C) 2011  ABRT team

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

#include <glib.h>

#ifndef EVENT_CONFIG_H
#define EVENT_CONFIG_H


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    OPTION_TYPE_TEXT,
    OPTION_TYPE_BOOL,
    OPTION_TYPE_PASSWORD,
    OPTION_TYPE_NUMBER,
    OPTION_TYPE_HINT_HTML,
    OPTION_TYPE_INVALID,
} option_type_t;

/*
 * struct to hold information about config options
 * it's supposed to hold information about:
 *   type -> which designates the widget used to display it and we can do some test based on the type
 *   label
 *   allowed value(s) -> regexp?
 *   name -> env variable name
 *   value -> value retrieved from the gui, so when we want to set the env
 *            evn variables, we can just traverse the list of the options
 *            and set the env variables according to name:value in this structure
 */
typedef struct
{
    char *eo_name; //name of the value which should be used for env variable
    char *eo_value;
    char *eo_label;
    char *eo_note_html;
    option_type_t eo_type;
    int eo_allow_empty;
    //char *description; //can be used as tooltip in gtk app
    //char *allowed_value;
    //int required;
} event_option_t;

event_option_t *new_event_option(void);
void free_event_option(event_option_t *p);

//structure to hold the option data
typedef struct
{
    char *screen_name; //ui friendly name of the event: "Bugzilla" "RedHat Support Upload"
    char *description; // "Report to..."/"Save to file". Should be one sentence, not long
    char *long_descr;  // Long(er) explanation, if needed

    char *ec_creates_items;
    char *ec_requires_items;
    char *ec_exclude_items_by_default;
    char *ec_include_items_by_default;
    char *ec_exclude_items_always;
    bool  ec_exclude_binary_items;

    GList *options;
} event_config_t;

event_config_t *new_event_config(void);
void free_event_config(event_config_t *p);


void load_event_description_from_file(event_config_t *event_config, const char* filename);

// (Re)loads data from /etc/abrt/events/*.{conf,xml}
void load_event_config_data(void);
/* Frees all loaded data */
void free_event_config_data(void);
event_config_t *get_event_config(const char *event_name);
event_option_t *get_event_option_from_list(const char *option_name, GList *event_options);

extern GHashTable *g_event_config_list;   // for iterating through entire list of all loaded configs

GList *export_event_config(const char *event_name);
void unexport_event_config(GList *env_list);

GHashTable *validate_event(const char *event_name);

#ifdef __cplusplus
}
#endif

#endif
