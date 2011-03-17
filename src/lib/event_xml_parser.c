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
#include "abrtlib.h"
#include "event_config.h"

#define EVENT_ELEMENT           "event"
#define LABEL_ELEMENT           "label"
#define DESCRIPTION_ELEMENT     "description"
#define ALLOW_EMPTY_ELEMENT     "allow-empty"
#define OPTION_ELEMENT          "option"
//#define ACTION_ELEMENT        "action"
#define NAME_ELEMENT            "name"
#define DEFAULT_VALUE_ELEMENT   "default-value"

struct my_parse_data
{
    event_config_t *event_config;
    event_option_t *cur_option;
};

static const char *const option_types[] =
{
    "text",
    "bool",
    "password",
    "number",
    NULL
};

static int cmp_event_option_name_with_string(gconstpointer a, gconstpointer b)
{
    return strcmp(((event_option_t *)a)->name, (char *)b);
}

static void consume_cur_option(struct my_parse_data *parse_data)
{
    event_option_t *opt = parse_data->cur_option;
    if (!opt)
        return;

    parse_data->cur_option = NULL;

    if (!opt->name)
    {
//TODO: "option w/o name" error msg?
        free_event_option(opt);
        return;
    }

    event_config_t *event_config = parse_data->event_config;
    GList *elem = g_list_find_custom(event_config->options, opt->name,
                                     &cmp_event_option_name_with_string);
    if (elem)
    {
        /* we already have option with such name */
        event_option_t *old_opt = elem->data;
        if (old_opt->value)
        {
            /* ...and it already has a value, which
             * overrides xml-defined default one:
             */
            free(opt->value);
            opt->value = old_opt->value;
            old_opt->value = NULL;
        }
        //log("xml: replacing '%s' value:'%s'->'%s'", opt->name, old_opt->value, opt->value);
        free_event_option(old_opt);
        elem->data = opt;
    }
    else
    {
        //log("xml: new value %s='%s'", opt->name, opt->value);
        event_config->options = g_list_append(event_config->options, opt);
    }
}

// Called for open tags <foo bar="baz">
static void start_element(GMarkupParseContext *context,
                  const gchar *element_name,
                  const gchar **attribute_names,
                  const gchar **attribute_values,
                  gpointer user_data,
                  GError **error)
{
    //log("start: %s", element_name);

    struct my_parse_data *parse_data = user_data;

    if (strcmp(element_name, OPTION_ELEMENT) == 0)
    {
        if (parse_data->cur_option)
        {
            error_msg("error, option nested in option");
            return;
        }

        event_option_t *opt = parse_data->cur_option = new_event_option();

        for (int i = 0; attribute_names[i] != NULL; ++i)
        {
            VERB2 log("attr: %s:%s", attribute_names[i], attribute_values[i]);
            if (strcmp(attribute_names[i], "name") == 0)
            {
                free(opt->name);
                opt->name = xstrdup(attribute_values[i]);
            }
            else if (strcmp(attribute_names[i], "type") == 0)
            {
                option_type_t type;
                for (type = OPTION_TYPE_TEXT; type < OPTION_TYPE_INVALID; ++type)
                {
                    if (strcmp(option_types[type], attribute_values[i]) == 0)
                        opt->type = type;
                }
            }
        }
    }
}

// Called for close tags </foo>
static void end_element(GMarkupParseContext *context,
                          const gchar         *element_name,
                          gpointer             user_data,
                          GError             **error)
{
    struct my_parse_data *parse_data = user_data;

    if (strcmp(element_name, OPTION_ELEMENT) == 0)
    {
        consume_cur_option(parse_data);
    }
    if (strcmp(element_name, EVENT_ELEMENT) == 0)
    {
        consume_cur_option(parse_data);
    }
}

// Called for character data
// text is not nul-terminated
static void text(GMarkupParseContext *context,
         const gchar         *text,
         gsize                text_len,
         gpointer             user_data,
         GError             **error)
{
    struct my_parse_data *parse_data = user_data;
    event_config_t *ui = parse_data->event_config;

    const gchar *inner_element = g_markup_parse_context_get_element(context);
    char *text_copy = xstrndup(text, text_len);
    event_option_t *opt = parse_data->cur_option;
    if (opt)
    {
        if (strcmp(inner_element, LABEL_ELEMENT) == 0)
        {
            VERB2 log("new label:'%s'", text_copy);
            free(opt->label);
            opt->label = text_copy;
            return;
        }
        /*
        * we can add a separate filed for the default value
        * in that case we can implement features like "reset to default value"
        * but for now using "value" should be enough and clients doesn't
        * have to know about the "defaul-value"
        */
        if (strcmp(inner_element, DEFAULT_VALUE_ELEMENT) == 0)
        {
            VERB2 log("default value:'%s'", text_copy);
            free(opt->value);
            opt->value = text_copy;
            return;
        }

        if (strcmp(inner_element, ALLOW_EMPTY_ELEMENT) == 0)
        {
            VERB2 log("allow-empty:'%s'", text_copy);
            opt->allow_empty = string_to_bool(text_copy);
            return;
        }
        /*
        if (strcmp(inner_element, DESCRIPTION_ELEMENT) == 0)
        {
            VERB2 log("tooltip:'%s'", text_copy);
            free(opt->description);
            opt->description = text_copy;
            return;
        }
        */
    }
    else
    {
        /* we're not in option, so the description is for the event */
        /*
        if (strcmp(inner_element, ACTION_ELEMENT) == 0)
        {
            VERB2 log("action description:'%s'", text_copy);
            free(ui->action);
            ui->action = text_copy;
            return;
        }
        */
        if (strcmp(inner_element, NAME_ELEMENT) == 0)
        {
            VERB2 log("event name:'%s'", text_copy);
            free(ui->screen_name);
            ui->screen_name = text_copy;
            return;
        }
        if (strcmp(inner_element, DESCRIPTION_ELEMENT) == 0)
        {
            VERB2 log("event description:'%s'", text_copy);
            free(ui->description);
            ui->description = text_copy;
            return;
        }
    }
    free(text_copy);
}

  // Called for strings that should be re-saved verbatim in this same
  // position, but are not otherwise interpretable.  At the moment
  // this includes comments and processing instructions.
  // text is not nul-terminated
static void passthrough(GMarkupParseContext *context,
                const gchar *passthrough_text,
                gsize text_len,
                gpointer user_data,
                GError **error)
{
    VERB2 log("passthrough");
}

// Called on error, including one set by other
// methods in the vtable. The GError should not be freed.
static void error(GMarkupParseContext *context,
          GError *error,
          gpointer user_data)
{
    error_msg("error in XML parsing");
}

/* this function takes 2 parameters
 * ui -> pointer to event_config_t
 * filename -> filename to read
 * event_config_t contains list of options, which is malloced by hits function
 * and must be freed by the caller
 */

void load_event_description_from_file(event_config_t *event_config, const char* filename)
{
    struct my_parse_data parse_data = { event_config, NULL };

    GMarkupParser parser;
    memset(&parser, 0, sizeof(parser)); /* just in case */
    parser.start_element = &start_element;
    parser.end_element = &end_element;
    parser.text = &text;
    parser.passthrough = &passthrough;
    parser.error = &error;

    GMarkupParseContext *context = g_markup_parse_context_new(
                    &parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                    &parse_data, /*GDestroyNotify:*/ NULL);

    FILE* fin = fopen(filename, "r");
    if (fin != NULL)
    {
        size_t read_bytes = 0;
        char buff[1024];
        while ((read_bytes = fread(buff, 1, 1024, fin)) != 0)
        {
            g_markup_parse_context_parse(context, buff, read_bytes, NULL);
        }
        fclose(fin);
    }

    g_markup_parse_context_free(context);

    consume_cur_option(&parse_data); /* just in case */
}
