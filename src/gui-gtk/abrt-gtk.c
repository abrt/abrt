#include <gtk/gtk.h>
#include "abrtlib.h"
#include "abrt-gtk.h"

static GtkListStore *dumps_list_store;
static GtkTreeIter iter___;

enum
{
    COLUMN_REPORTED,
    COLUMN_APPLICATION,
    COLUMN_HOSTNAME,
    COLUMN_LATEST_CRASH_STR,
    COLUMN_LATEST_CRASH,
    COLUMN_DUMP_DIR,
    NUM_COLUMNS
};

/*
void gtk_tree_model_get_value(GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         gint column,
                                         GValue *value);
*/

static void on_row_activated_cb(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *dump_list_store = gtk_tree_view_get_model(tree_view);
        gtk_tree_model_get_iter(dump_list_store, &iter, path);
        GValue d_dir = { 0 };
        if (gtk_tree_selection_get_selected(selection, &dump_list_store, &iter) == TRUE)
        {
            gtk_tree_model_get_value(dump_list_store, &iter, COLUMN_DUMP_DIR, &d_dir);
            g_print("CALL: run_event(%s)\n", g_value_get_string(&d_dir));
        }
    }
}

void add_directory_to_dirlist(const char *dirname)
{
    struct dump_dir *dd = dd_opendir(dirname, DD_OPEN_READONLY);
    if (!dd)
        return;

    time_t time = atoi(dd_load_text(dd, FILENAME_TIME));
    struct tm * ptm = localtime(&time);
    char time_buf[60];
    size_t time_len = strftime(time_buf, 59, "%c",  ptm);
    time_buf[time_len] = '\0';

    gtk_list_store_append(dumps_list_store, &iter___);
    gtk_list_store_set(dumps_list_store, &iter___,
                          COLUMN_REPORTED, "??",
                          COLUMN_APPLICATION, dd_load_text(dd, FILENAME_EXECUTABLE),
                          COLUMN_HOSTNAME, dd_load_text(dd, FILENAME_HOSTNAME),
                          //OPTION: time format
                          COLUMN_LATEST_CRASH_STR, time_buf,
                          COLUMN_LATEST_CRASH, (int)time,
                          COLUMN_DUMP_DIR, dirname,
                          -1);
    dd_close(dd);
    VERB1 log("added: %s\n", dirname);
}

static void add_columns(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* column reported */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Reported"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_REPORTED,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_REPORTED);
    gtk_tree_view_append_column(treeview, column);

    /* column for executable path */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Application"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_APPLICATION,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_APPLICATION);
    gtk_tree_view_append_column(treeview, column);

    /* column for hostname */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Hostname"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_HOSTNAME,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_HOSTNAME);
    gtk_tree_view_append_column(treeview, column);

    /* column for the date of the last crash */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Last Crash"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_LATEST_CRASH_STR,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_LATEST_CRASH);
    gtk_tree_view_append_column(treeview, column);

}

GtkWidget *create_main_window(void)
{
    /* main window */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (!window)
        die_out_of_memory();

    gtk_window_set_default_size(GTK_WINDOW(window), 600, 700);
    gtk_window_set_title(GTK_WINDOW(window), _("Automatic Bug Reporting Tool"));
    gtk_window_set_icon_name(GTK_WINDOW(window), "abrt");
    //quit when user closes the main window
    g_signal_connect(window, "destroy", gtk_main_quit, NULL);

    /* main pane
     * holds the textview with dump list and a vbox with bug details
     * and bug details (icon, comment, howto, cmdline, etc ...)
     */
    GtkWidget *main_pane = gtk_vpaned_new();
    GtkWidget *dump_list_sw =  gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(dump_list_sw),
                                           GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dump_list_sw),
                                          GTK_POLICY_AUTOMATIC,
                                          GTK_POLICY_AUTOMATIC);
    GtkWidget *dump_tv = gtk_tree_view_new();
    add_columns(GTK_TREE_VIEW(dump_tv));
    dumps_list_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, /* reported */
                                                       G_TYPE_STRING, /* executable */
                                                       G_TYPE_STRING, /* hostname */
                                                       G_TYPE_STRING, /* time */
                                                       G_TYPE_INT,    /* unix time - used for sort */
                                                       G_TYPE_STRING);/* dump dir path */
    //if (dumps_list_store == NULL)
    //    return NULL;
    gtk_tree_view_set_model(GTK_TREE_VIEW(dump_tv), GTK_TREE_MODEL(dumps_list_store));
    g_signal_connect(dump_tv, "row-activated", G_CALLBACK(on_row_activated_cb), NULL);
    gtk_container_add(GTK_CONTAINER(dump_list_sw), dump_tv);

    /* dump details */
    GtkWidget *dump_details_sw =  gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dump_details_sw),
                                          GTK_POLICY_NEVER,
                                          GTK_POLICY_AUTOMATIC);

    GtkWidget *details_vbox = gtk_vbox_new(false, 0);
    GtkWidget *icon_name_hbox = gtk_hbox_new(false, 0);
    GtkWidget *details_hbox = gtk_hbox_new(true, 0);
    GtkWidget *package_icon = gtk_image_new_from_stock("gtk-missing-image", GTK_ICON_SIZE_DIALOG);


    gtk_box_pack_start(GTK_BOX(icon_name_hbox), package_icon, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(details_vbox), icon_name_hbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(details_vbox), details_hbox, FALSE, TRUE, 0);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(dump_details_sw), details_vbox);

    /* add dump list and dump details to pane */
    gtk_paned_pack1(GTK_PANED(main_pane), dump_list_sw, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(main_pane), dump_details_sw, FALSE, FALSE);




    GtkWidget *main_vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), main_pane, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);
    return window;
}
