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
#include <gtk/gtk.h>
#include "libreport-gtk.h"

static GtkWindow *g_event_list_window;
static GList *option_widget_list;
GtkWindow *g_parent_window;

enum
{
    COLUMN_EVENT_UINAME,
    COLUMN_EVENT_NAME,
    NUM_COLUMNS
};

typedef struct
{
    event_option_t *option;
    GtkWidget *widget;
} option_widget_t;

static void show_event_config_dialog(const char *event_name);

static GtkWidget *gtk_label_new_justify_left(const gchar *label_str)
{
    GtkWidget *label = gtk_label_new(label_str);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), /*xalign:*/ 0, /*yalign:*/ 0.5);
    /* Make some space between label and input field to the right of it: */
    gtk_misc_set_padding(GTK_MISC(label), /*xpad:*/ 5, /*ypad:*/ 0);
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

static unsigned grow_table_by_1(GtkTable *table)
{
    guint rows, columns;
    //needs gtk 2.22: gtk_table_get_size(table, &rows, &columns);
    g_object_get(table, "n-rows", &rows, NULL);
    g_object_get(table, "n-columns", &columns, NULL);
    gtk_table_resize(table, rows + 1, columns);
    return rows;
}

static void add_option_to_table(gpointer data, gpointer user_data)
{
    event_option_t *option = data;
    GtkTable *option_table = user_data;

    GtkWidget *label;
    GtkWidget *option_input;
    unsigned last_row;

    char *option_label;
    if (option->eo_label != NULL)
        option_label = xstrdup(option->eo_label);
    else
    {
        option_label = xstrdup(option->eo_name ? option->eo_name : "");
        /* Replace '_' with ' ' */
        char *p = option_label - 1;
        while (*++p)
            if (*p == '_')
                *p = ' ';
    }

    switch (option->eo_type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
        case OPTION_TYPE_PASSWORD:
            last_row = grow_table_by_1(option_table);
            label = gtk_label_new_justify_left(option_label);
            gtk_table_attach(option_table, label,
                             /*left,right_attach:*/ 0, 1,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
            option_input = gtk_entry_new();
            if (option->eo_value != NULL)
                gtk_entry_set_text(GTK_ENTRY(option_input), option->eo_value);
            gtk_table_attach(option_table, option_input,
                             /*left,right_attach:*/ 1, 2,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL | GTK_EXPAND, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
            add_option_widget(option_input, option);
            if (option->eo_type == OPTION_TYPE_PASSWORD)
            {
                gtk_entry_set_visibility(GTK_ENTRY(option_input), 0);
                last_row = grow_table_by_1(option_table);
                GtkWidget *pass_cb = gtk_check_button_new_with_label(_("Show password"));
                gtk_table_attach(option_table, pass_cb,
                             /*left,right_attach:*/ 1, 2,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
                g_signal_connect(pass_cb, "toggled", G_CALLBACK(on_show_pass_cb), option_input);
            }
            break;

        case OPTION_TYPE_HINT_HTML:
            label = gtk_label_new(option_label);
            gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
            gtk_misc_set_alignment(GTK_MISC(label), /*x,yalign:*/ 0.0, 0.0);
            make_label_autowrap_on_resize(GTK_LABEL(label));

            last_row = grow_table_by_1(option_table);
            gtk_table_attach(option_table, label,
                             /*left,right_attach:*/ 0, 2,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
            break;

        case OPTION_TYPE_BOOL:
            last_row = grow_table_by_1(option_table);
            option_input = gtk_check_button_new_with_label(option_label);
            gtk_table_attach(option_table, option_input,
                             /*left,right_attach:*/ 0, 2,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
            if (option->eo_value != NULL)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option_input),
                                    string_to_bool(option->eo_value));
            add_option_widget(option_input, option);
            break;

        default:
            //option_input = gtk_label_new_justify_left("WTF?");
            log("unsupported option type");
            free(option_label);
            return;
    }

    if (option->eo_note_html)
    {
        label = gtk_label_new(option->eo_note_html);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), /*x,yalign:*/ 0.0, 0.0);
        make_label_autowrap_on_resize(GTK_LABEL(label));

        last_row = grow_table_by_1(option_table);
        gtk_table_attach(option_table, label,
                             /*left,right_attach:*/ 1, 2,
                             /*top,bottom_attach:*/ last_row, last_row+1,
                             /*x,yoptions:*/ GTK_FILL, GTK_FILL,
                             /*x,ypadding:*/ 0, 0);
    }

    free(option_label);
}

static void on_close_event_list_cb(GtkWidget *button, gpointer user_data)
{
    GtkWidget *window = (GtkWidget *)user_data;
    gtk_widget_destroy(window);
}

static char *get_event_name_from_row(GtkTreeView *treeview)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    char *event_name = NULL;
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            GValue value = { 0 };
            gtk_tree_model_get_value(store, &iter, COLUMN_EVENT_NAME, &value);
            event_name = (char *)g_value_get_string(&value);
        }
    }
    return event_name;
}

static void on_configure_event_cb(GtkWidget *button, gpointer user_data)
{
    GtkTreeView *events_tv = (GtkTreeView *)user_data;
    char *event_name = get_event_name_from_row(events_tv);
    if (event_name != NULL)
        show_event_config_dialog(event_name);
    //else
    //    error_msg(_("Please select a plugin from the list to edit its options."));
}

static void on_event_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    char *event_name = get_event_name_from_row(treeview);
    event_config_t *ec = get_event_config(event_name);
    if (ec->options != NULL) //We need to have some options to show
        show_event_config_dialog(event_name);
}

static void on_event_row_changed_cb(GtkTreeView *treeview, gpointer user_data)
{
    event_config_t *ec = get_event_config(get_event_name_from_row(treeview));
    gtk_widget_set_sensitive(GTK_WIDGET(user_data), ec->options != NULL);
}

static void add_event_to_liststore(gpointer key, gpointer value, gpointer user_data)
{
    GtkListStore *events_list_store = (GtkListStore *)user_data;
    event_config_t *ec = (event_config_t *)value;

    char *event_label;
    if (ec->screen_name != NULL && ec->description != NULL)
        event_label = xasprintf("<b>%s</b>\n%s", ec->screen_name, ec->description);
    else
        //if event has no xml description
        event_label = xasprintf("<b>%s</b>\nNo description available", key);

    GtkTreeIter iter;
    gtk_list_store_append(events_list_store, &iter);
    gtk_list_store_set(events_list_store, &iter,
                      COLUMN_EVENT_UINAME, event_label,
                      COLUMN_EVENT_NAME, key,
                      -1);
    free(event_label);
}

static void save_value_from_widget(gpointer data, gpointer user_data)
{
    option_widget_t *ow = (option_widget_t *)data;

    const char *val = NULL;
    switch (ow->option->eo_type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
        case OPTION_TYPE_PASSWORD:
            val = (char *)gtk_entry_get_text(GTK_ENTRY(ow->widget));
            break;
        case OPTION_TYPE_BOOL:
            val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->widget)) ? "yes" : "no";
            break;
        default:
            log("unsupported option type");
    }
    if (val)
    {
        free(ow->option->eo_value);
        ow->option->eo_value = xstrdup(val);
        VERB1 log("saved: %s:%s", ow->option->eo_name, ow->option->eo_value);
    }
}

static void dehydrate_config_dialog()
{
    if (option_widget_list != NULL)
        g_list_foreach(option_widget_list, &save_value_from_widget, NULL);
}

static void show_event_config_dialog(const char *event_name)
{
    if (option_widget_list != NULL)
    {
        g_list_free(option_widget_list);
        option_widget_list = NULL;
    }

    event_config_t *event = get_event_config(event_name);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
                        /*title:*/ event->screen_name ? event->screen_name : event_name,
                        g_event_list_window,
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_OK,
                        GTK_RESPONSE_APPLY,
                        NULL);
    if (g_event_list_window != NULL)
    {
        gtk_window_set_icon_name(GTK_WINDOW(dialog),
                gtk_window_get_icon_name(g_event_list_window));
    }

    GtkWidget *option_table = gtk_table_new(/*rows*/ 0, /*cols*/ 2, /*homogeneous*/ FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(option_table), 2);
    g_list_foreach(event->options, &add_option_to_table, option_table);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), option_table, false, false, 20);
    gtk_widget_show_all(option_table);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_APPLY)
    {
        dehydrate_config_dialog();
        abrt_keyring_save_settings(event_name);
    }
    //else if (result == GTK_RESPONSE_CANCEL)
    //    log("log");
    gtk_widget_destroy(dialog);
}

void show_events_list_dialog(GtkWindow *parent)
{
    /*remove this line if we want to reload the config
     *everytime we show the config dialog
     */
    if (g_event_config_list == NULL)
    {
        load_event_config_data();
        load_event_config_data_from_keyring();
    }

    GtkWidget *event_list_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_event_list_window = (GtkWindow*)event_list_window;
    gtk_window_set_title(g_event_list_window, _("Event Configuration"));
    gtk_window_set_default_size(g_event_list_window, 450, 400);
    gtk_window_set_position(g_event_list_window, parent ? GTK_WIN_POS_CENTER_ON_PARENT : GTK_WIN_POS_CENTER);
    if (parent != NULL)
    {
        gtk_window_set_transient_for(g_event_list_window, parent);
        // modal = parent window can't steal focus
        gtk_window_set_modal(g_event_list_window, true);
        gtk_window_set_icon_name(g_event_list_window,
            gtk_window_get_icon_name(parent));
    }

    GtkWidget *main_vbox = gtk_vbox_new(0, 0);
    GtkWidget *events_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(events_scroll),
                                    GTK_POLICY_NEVER,
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
                                                 COLUMN_EVENT_UINAME,
                                                 NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    g_object_set(G_OBJECT(renderer), "wrap-mode", PANGO_WRAP_WORD, NULL);
    g_object_set(G_OBJECT(renderer), "wrap-width", 440, NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_EVENT_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(events_tv), column);
    /* "Please draw rows in alternating colors": */
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(events_tv), TRUE);
    // TODO: gtk_tree_view_set_headers_visible(FALSE)? We have only one column anyway...

    /* Create data store for the list and attach it
     * COLUMN_EVENT_UINAME -> name+description
     * COLUMN_EVENT_NAME -> event name so we can retrieve it from the row
     */
    GtkListStore *events_list_store = gtk_list_store_new(NUM_COLUMNS,
                                                G_TYPE_STRING, /* Event name + description */
                                                G_TYPE_STRING  /* event name */
    );
    gtk_tree_view_set_model(GTK_TREE_VIEW(events_tv), GTK_TREE_MODEL(events_list_store));

    g_hash_table_foreach(g_event_config_list,
                        &add_event_to_liststore,
                        events_list_store);
//TODO: can unref events_list_store? treeview holds one ref.

    /* Double click/Enter handler */
    g_signal_connect(events_tv, "row-activated", G_CALLBACK(on_event_row_activated_cb), NULL);

    gtk_container_add(GTK_CONTAINER(events_scroll), events_tv);

    GtkWidget *configure_event_btn = gtk_button_new_with_mnemonic(_("Configure E_vent"));
    gtk_widget_set_sensitive(configure_event_btn, false);
    g_signal_connect(configure_event_btn, "clicked", G_CALLBACK(on_configure_event_cb), events_tv);
    g_signal_connect(events_tv, "cursor-changed", G_CALLBACK(on_event_row_changed_cb), configure_event_btn);

    GtkWidget *close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_event_list_cb), g_event_list_window);

    GtkWidget *btnbox = gtk_hbutton_box_new();
    gtk_box_pack_end(GTK_BOX(btnbox), close_btn, false, false, 0);
    gtk_box_pack_end(GTK_BOX(btnbox), configure_event_btn, false, false, 0);

    gtk_box_pack_start(GTK_BOX(main_vbox), events_scroll, true, true, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), btnbox, false, false, 0);

    gtk_container_add(GTK_CONTAINER(event_list_window), main_vbox);

    gtk_widget_show_all(event_list_window);
}

static void show_event_opt_error_dialog(const char *event_name)
{
    event_config_t *ec = get_event_config(event_name);
    char *message = xasprintf(_("Wrong settings detected for %s, "
                              "reporting will probably fail if you continue "
                              "with the current configuration."),
                               ec->screen_name);
    char *markup_message = xasprintf(_("Wrong settings detected for <b>%s</b>, "
                              "reporting will probably fail if you continue "
                              "with the current configuration."),
                               ec->screen_name);
    GtkWidget *wrong_settings = gtk_message_dialog_new(g_parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_CLOSE,
        message);
    gtk_window_set_transient_for(GTK_WINDOW(wrong_settings), g_parent_window);
    free(message);
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(wrong_settings),
                                    markup_message);
    free(markup_message);
    gtk_dialog_run(GTK_DIALOG(wrong_settings));
    gtk_widget_destroy(wrong_settings);
}

//TODO: move this code to its only callsite?
// (in which case, move show_event_opt_error_dialog and g_parent_window too)
void g_validate_event(const char* event_name)
{
    GHashTable *errors = validate_event(event_name);
    if (errors != NULL)
        show_event_opt_error_dialog(event_name);
}
