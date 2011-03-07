#include <glib.h>

typedef enum
{
    OPTION_TYPE_TEXT,
    OPTION_TYPE_BOOL,
    OPTION_TYPE_PASSWORD,
    OPTION_TYPE_NUMBER,
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
    const char* label;
    const char* name; //name of the value which should be used for env variable
    char *value;
    option_type_t type;
    const char* description; //can be used as tooltip in gtk app
    const char* allowed_value;
    int required;
} event_option_obj_t;

//structure to hold the option data
typedef struct
{
    const char *name;  //name of the event "Bugzilla" "RedHat Support Uploader"
    const char *title; //window title - not used right now, maybe the "name" is enough?
    const char *action;//action description to show in gui like: Upload report to the Red Hat bugzilla"
    GList *options;
} event_obj_t;
