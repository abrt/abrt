#include "abrtlib.h"
#include <gtk/gtk.h>

static GtkWidget *option_table;
static GtkWidget *parent_dialog;
static GList *option_widget_list;
static int last_row = 0;

enum
{
    COLUMN_EVENT_NAME,
    COLUMN_EVENT,
    NUM_COLUMNS
};

typedef struct
{
    event_option_t *option;
    GtkWidget *widget;
} option_widget_t;

static void show_event_config_dialog(event_config_t* event);

static void show_error_message(const char* message)
{
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               message
                                               );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

GtkWidget *gtk_label_new_justify_left(const gchar *label_str)
{
    GtkWidget *label = gtk_label_new(label_str);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), /*xalign:*/ 0, /*yalign:*/ 0.5);
    return label;
}

static void add_option_widget(GtkWidget *widget, event_option_t *option)
{
    option_widget_t *ow = (option_widget_t *)xmalloc(sizeof(option_widget_t));
    ow->widget = widget;
    ow->option = option;
    option_widget_list = g_list_prepend(option_widget_list, ow);
}

static void on_show_pass_cb(GtkToggleButton *tb, gpointer user_data)
{
    GtkEntry *entry = (GtkEntry *)user_data;
    gtk_entry_set_visibility(entry, gtk_toggle_button_get_active(tb));
}

static void add_option_to_dialog(event_option_t *option)
{
    GtkWidget *label;
    GtkWidget *option_input;
    GtkWidget *option_hbox = gtk_hbox_new(FALSE, 0);
    char *option_label;
    if(option->label != NULL)
        option_label = option->label;
    else
        option_label = option->name;

    switch(option->type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
            label = gtk_label_new_justify_left(option_label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            if(option->value != NULL)
                gtk_entry_set_text(GTK_ENTRY(option_input), option->value);
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            add_option_widget(option_input, option);
            break;
        case OPTION_TYPE_BOOL:
            option_input = gtk_check_button_new_with_label(option_label);
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             0, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            if(option->value != NULL)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option_input),
                                    (strcmp("yes",option->value)==0));
            add_option_widget(option_input, option);
            break;
        case OPTION_TYPE_PASSWORD:
            label = gtk_label_new_justify_left(option_label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            if(option->value != NULL)
                gtk_entry_set_text(GTK_ENTRY(option_input), option->value);
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            gtk_entry_set_visibility(GTK_ENTRY(option_input), 0);
            add_option_widget(option_input, option);
            last_row++;
            GtkWidget *pass_cb = gtk_check_button_new_with_label(_("Show password"));
            gtk_table_attach(GTK_TABLE(option_table), pass_cb,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            g_signal_connect(pass_cb, "toggled", G_CALLBACK(on_show_pass_cb), option_input);
            break;
        default:
            //option_input = gtk_label_new_justify_left("WTF?");
            g_print("unsupported option type\n");
    }
    last_row++;

    gtk_widget_show_all(GTK_WIDGET(option_hbox));
}

static void add_option(gpointer data, gpointer user_data)
{
    event_option_t * option = (event_option_t *)data;
    add_option_to_dialog(option);
}

static void on_close_event_list_cb(GtkWidget *button, gpointer user_data)
{
    GtkWidget *window = (GtkWidget *)user_data;
    gtk_widget_destroy(window);
}

static event_config_t *get_event_config_from_row(GtkTreeView *treeview)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    event_config_t *event_config = NULL;
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            GValue value = { 0 };
            gtk_tree_model_get_value(store, &iter, COLUMN_EVENT, &value);
            event_config = (event_config_t*)g_value_get_pointer(&value);
        }
    }
    return event_config;
}

static void on_configure_event_cb(GtkWidget *button, gpointer user_data)
{
    GtkTreeView *events_tv = (GtkTreeView *)user_data;
    event_config_t *ec = get_event_config_from_row(events_tv);
    if(ec != NULL)
        show_event_config_dialog(ec);
    else
        show_error_message(_("Please select a plugin from the list to edit its options."));
}

static void on_event_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    event_config_t *ec = get_event_config_from_row(treeview);
    if(ec->options != NULL)
        show_event_config_dialog(ec);
}

static void on_event_row_changed_cb(GtkTreeView *treeview, gpointer user_data)
{
    event_config_t *ec = get_event_config_from_row(treeview);
    gtk_widget_set_sensitive(GTK_WIDGET(user_data), ec->options != NULL);
}

static void add_event_to_liststore(gpointer key, gpointer value, gpointer user_data)
{
    GtkListStore *events_list_store = (GtkListStore *)user_data;
    event_config_t *ec = (event_config_t *)value;
    char *event_label = NULL;
    if(ec->screen_name != NULL && ec->description != NULL)
        event_label = xasprintf("<b>%s</b>\n%s", ec->screen_name, ec->description);
    else
        //if event has no xml description
        event_label = xasprintf("<b>%s</b>\nNo description available", key);

    GtkTreeIter iter;
    gtk_list_store_append(events_list_store, &iter);
    gtk_list_store_set(events_list_store, &iter,
                      COLUMN_EVENT_NAME, event_label,
                      COLUMN_EVENT, value,
                      -1);
}

static void save_value_from_widget(gpointer data, gpointer user_data)
{
    option_widget_t *ow = (option_widget_t *)data;
    switch(ow->option->type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
        case OPTION_TYPE_PASSWORD:
            ow->option->value = (char *)gtk_entry_get_text(GTK_ENTRY(ow->widget));
            break;
        case OPTION_TYPE_BOOL:
            ow->option->value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->widget)) ? xstrdup("yes") : xstrdup("no");
            break;
        default:
            g_print("unsupported option type\n");
    }
    VERB1 log("saved: %s:%s", ow->option->name, ow->option->value);
}

static void dehydrate_config_dialog()
{
    if(option_widget_list != NULL)
        g_list_foreach(option_widget_list, &save_value_from_widget, NULL);
}

static void show_event_config_dialog(event_config_t* event)
{
    if(option_widget_list != NULL)
    {
        g_list_free(option_widget_list);
        option_widget_list = NULL;
    }

    char *title;
    if(event->screen_name != NULL)
        title = event->screen_name;
    else
        title = _("Event Configuration");

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
                        title,
                        GTK_WINDOW(parent_dialog),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_OK,
                        GTK_RESPONSE_APPLY,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        NULL);
    if(parent_dialog != NULL)
    {
        gtk_window_set_icon_name(GTK_WINDOW(dialog),
                gtk_window_get_icon_name(GTK_WINDOW(parent_dialog)));
    }
    int length = g_list_length(event->options);
    option_table = gtk_table_new(length, 2, 0);
    g_list_foreach(event->options, &add_option, NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), option_table, false, false, 20);
    gtk_widget_show_all(option_table);
    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if(result == GTK_RESPONSE_APPLY)
        dehydrate_config_dialog();
    else if(result == GTK_RESPONSE_CANCEL)
        g_print("cancel\n");
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void show_events_list_dialog(GtkWindow *parent)
{
        /*remove this line if we want to reload the config
         *everytime we show the config dialog
         */
        if(g_event_config_list == NULL)
            load_event_config_data();
        if(g_event_config_list == NULL)
        {
            VERB1 log("can't load event's config\n");
            show_error_message(_("Can't load event descriptions"));
            return;
        }
        parent_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(parent_dialog), _("Events"));
        gtk_window_set_default_size(GTK_WINDOW(parent_dialog), 450, 400);
        if(parent != NULL)
        {
            gtk_window_set_transient_for(GTK_WINDOW(parent_dialog), parent);
            // modal = parent window can't steal focus
            gtk_window_set_modal(GTK_WINDOW(parent_dialog), true);
            gtk_window_set_icon_name(GTK_WINDOW(parent_dialog),
                gtk_window_get_icon_name(parent));
        }

        GtkWidget *main_vbox = gtk_vbox_new(0, 0);
        GtkWidget *events_scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(events_scroll),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        /* event list treeview */
        GtkWidget *events_tv = gtk_tree_view_new();
        /* column with event name and description */
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        /* add column to tree view */
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Event"),
                                                     renderer,
                                                     "markup",
                                                     COLUMN_EVENT_NAME,
                                                     NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id(column, COLUMN_EVENT_NAME);
        gtk_tree_view_append_column(GTK_TREE_VIEW(events_tv), column);

        /* Create data store for the list and attach it
        * COLUMN_EVENT_NAME -> name+description
        * COLUMN_EVENT -> event_conf_t* so we can retrieve the event_config from the row
        */
        GtkListStore *events_list_store = gtk_list_store_new(NUM_COLUMNS,
                                                    G_TYPE_STRING, /* Event name + description */
                                                    G_TYPE_POINTER);
        gtk_tree_view_set_model(GTK_TREE_VIEW(events_tv), GTK_TREE_MODEL(events_list_store));

        g_hash_table_foreach(g_event_config_list,
                            &add_event_to_liststore,
                            events_list_store);

        /* Double click/Enter handler */
        g_signal_connect(events_tv, "row-activated", G_CALLBACK(on_event_row_activated_cb), NULL);

        gtk_container_add(GTK_CONTAINER(events_scroll), events_tv);

        GtkWidget *configure_event_btn = gtk_button_new_with_mnemonic(_("Configure E_vent"));
        gtk_widget_set_sensitive(configure_event_btn, false);
        g_signal_connect(configure_event_btn, "clicked", G_CALLBACK(on_configure_event_cb), events_tv);
        g_signal_connect(events_tv, "cursor-changed", G_CALLBACK(on_event_row_changed_cb), configure_event_btn);

        GtkWidget *close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_event_list_cb), parent_dialog);

        GtkWidget *btnbox = gtk_hbutton_box_new();
        gtk_box_pack_end(GTK_BOX(btnbox), configure_event_btn, false, false, 0);
        gtk_box_pack_end(GTK_BOX(btnbox), close_btn, false, false, 0);

        gtk_box_pack_start(GTK_BOX(main_vbox), events_scroll, true, true, 10);
        gtk_box_pack_start(GTK_BOX(main_vbox), btnbox, false, false, 0);
        gtk_container_add(GTK_CONTAINER(parent_dialog), main_vbox);

        gtk_widget_show_all(parent_dialog);
}
