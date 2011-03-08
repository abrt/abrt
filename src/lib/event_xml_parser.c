//#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "abrtlib.h"
#include "event_config.h"

#define EVENT_ELEMENT       "event"
#define LABEL_ELEMENT       "label"
#define DESCRIPTION_ELEMENT "description"
#define ALLOW_EMPTY_ELEMENT "allow-empty"
#define OPTION_ELEMENT      "option"
#define ACTION_ELEMENT      "action"
#define NAME_ELEMENT        "name"

int in_option = 0;

static const char *option_types[] =
{
    "text",
    "bool",
    "password",
    "number",
    NULL
};

// Called for open tags <foo bar="baz">
void start_element(GMarkupParseContext *context,
                  const gchar *element_name,
                  const gchar **attribute_names,
                  const gchar **attribute_values,
                  gpointer user_data,
                  GError **error)
{
    //g_print("start: %s\n", element_name);

    event_config_t *ui = (event_config_t *)user_data;

    if(strcmp(element_name, OPTION_ELEMENT) == 0)
    {
        if(in_option == 0)
        {
            in_option = 1;
            event_option_t *option = (event_option_t *)malloc(sizeof(event_option_t));
            //we need to prepend, so ui->options always points to the last created option
            VERB2 log("adding option\n");
            ui->options = g_list_prepend(ui->options, option);

            int i;
            for(i = 0; attribute_names[i] != NULL; ++i)
            {
                VERB2 log("attr: %s:%s\n", attribute_names[i], attribute_values[i]);
                if(strcmp(attribute_names[i], "name") == 0)
                    option->name = strdup(attribute_values[i]);
                if(strcmp(attribute_names[i], "type") == 0)
                {
                    option_type_t type = 0;
                    for(type = OPTION_TYPE_TEXT; type < OPTION_TYPE_INVALID; ++type)
                    {
                        if(strcmp(option_types[type], attribute_values[i]) == 0)
                            option->type = type;
                    }
                }
            }
        }
        else
        {
            g_print("error, option nested in option!\n");
            exit(-127);
        }
    }

}

 // Called for close tags </foo>
void end_element(GMarkupParseContext *context,
                          const gchar         *element_name,
                          gpointer             user_data,
                          GError             **error)
{
    event_config_t *ui = (event_config_t *)user_data;
    if(strcmp(element_name, OPTION_ELEMENT) == 0)
    {
        in_option = 0;
    }
    if(strcmp(element_name, EVENT_ELEMENT) == 0)
    {
        //we need to revers the list, because we we're prepending
        ui->options = g_list_reverse(ui->options);
        in_option = 0;
    }
}

  // Called for character data
  // text is not nul-terminated
void text(GMarkupParseContext *context,
         const gchar         *text,
         gsize                text_len,
         gpointer             user_data,
         GError             **error)
{
    event_config_t *ui = (event_config_t *)user_data;
    const gchar * inner_element = g_markup_parse_context_get_element(context);
    char *_text = (char *)malloc(text_len+1);
    strncpy(_text, text, text_len);
    _text[text_len] = '\0';
    if(in_option == 1)
    {
        event_option_t *option = (event_option_t *)((ui->options)->data);
        if(strcmp(inner_element, LABEL_ELEMENT) == 0)
        {
            VERB2 log("\tnew label: \t\t%s\n", _text);
            option->label = _text;
        }
        if(strcmp(inner_element, DESCRIPTION_ELEMENT) == 0)
        {
            VERB2 log("\ttooltip: \t\t%s\n", _text);
            option->description = _text;
        }
    }
    else
    {
        /* we're not in option, so the description is for the event */
        if(strcmp(inner_element, ACTION_ELEMENT) == 0)
        {
            //g_print("\taction description: \t%s\n", _text);
            ui->action = _text;
        }
        if(strcmp(inner_element, NAME_ELEMENT) == 0)
        {
            //g_print("\tevent name: \t\t%s\n", _text);
            ui->name = _text;
        }
    }
    //will be freed when the event_option is freed
    //free(_text);
}

  // Called for strings that should be re-saved verbatim in this same
  // position, but are not otherwise interpretable.  At the moment
  // this includes comments and processing instructions.
  // text is not nul-terminated
void passthrough(GMarkupParseContext *context,
                const gchar *passthrough_text,
                gsize text_len,
                gpointer user_data,
                GError **error)
{
    VERB2 log("passthrough\n");
}

// Called on error, including one set by other
// methods in the vtable. The GError should not be freed.
void error(GMarkupParseContext *context,
          GError *error,
          gpointer user_data)
{
    g_print("error\n");
}

/* this function takes 2 parameters
  * ui -> pointer to event_config_t
  * filename -> filename to read
 * event_config_t contains list of options, which is malloced by hits function
 * and must be freed by the caller
 */

void load_event_description_from_file(event_config_t *event_config, const char* filename)
{
    GMarkupParser parser;
    parser.start_element = &start_element;
    parser.end_element = &end_element;
    parser.text = &text;
    parser.passthrough = &passthrough;
    parser.error = &error;
    GMarkupParseContext *context = g_markup_parse_context_new(
                    &parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                    event_config, NULL);
    FILE* fin = fopen(filename, "r");
    size_t read_bytes = 0;
    char buff[1024] = {'\0'};
    while((read_bytes = fread(buff, 1, 1024, fin)))
    {
        g_markup_parse_context_parse(context, buff, read_bytes, NULL);
    }
    fclose(fin);
    free(context);
}
