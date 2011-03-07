#include "abrtlib.h"
#include <gtk/gtk.h>

static GtkWidget *option_table;
static int last_row = 0;

GtkWidget *gtk_label_new_justify_left(const gchar *label_str)
{
    GtkWidget *label = gtk_label_new(label_str);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), /*xalign:*/ 0, /*yalign:*/ 0.5);
    return label;
}

void add_option_to_dialog(event_option_t *option)
{

    GtkWidget *label;
    GtkWidget *option_input;
    GtkWidget *option_hbox = gtk_hbox_new(FALSE, 0);
    switch(option->type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
            label = gtk_label_new_justify_left(option->label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);

            break;
        case OPTION_TYPE_BOOL:
            option_input = gtk_check_button_new_with_label(option->label);
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             0, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            break;
        case OPTION_TYPE_PASSWORD:
            label = gtk_label_new_justify_left(option->label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);

            gtk_entry_set_visibility(GTK_ENTRY(option_input), 0);
            break;
        default:
            option_input = gtk_label_new_justify_left("WTF?");
            g_print("unsupported option type\n");
    }
    last_row++;

    gtk_widget_show_all(GTK_WIDGET(option_hbox));
}

void print_option(gpointer data, gpointer user_data)
{
    event_option_t * option = (event_option_t *)data;
    /*
    g_print("option:\n");
    g_print("\tlabel: %s\n", option->label);
    g_print("\tenv name: %s\n", option->name);
    g_print("\ttooltip: %s\n", option->description);
    g_print("\ttype: %i\n", option->type);
    */
    add_option_to_dialog(option);
}

void show_event_config_dialog(const char* event_name)
{
    event_config_t ui;
    ui.options = NULL;
    load_event_description_from_file(&ui, "Bugzilla.xml");
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
                        ui.name,
                        NULL,
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_OK,
                        GTK_RESPONSE_APPLY,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        NULL);
    int length = g_list_length(ui.options);
    //g_print("%i\n", length);
    option_table = gtk_table_new(length, 2, 0);
    g_list_foreach(ui.options, &print_option, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), option_table, 0, 0, 10);
    gtk_widget_show_all(option_table);
    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if(result == GTK_RESPONSE_APPLY)
        g_print("apply\n");
    else if(result == GTK_RESPONSE_CANCEL)
        g_print("cancel\n");
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
