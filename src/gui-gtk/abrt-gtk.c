#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "abrt-gtk.h"

static GtkListStore *s_dumps_list_store;
static GtkWidget *s_treeview;

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

void add_directory_to_dirlist(const char *dirname)
{
    struct dump_dir *dd = dd_opendir(dirname, DD_OPEN_READONLY);
    if (!dd)
        return;

    time_t time = atoi(dd_load_text(dd, FILENAME_TIME));
    struct tm *ptm = localtime(&time);
    char time_buf[60];
    size_t time_len = strftime(time_buf, sizeof(time_buf)-1, "%c", ptm);
    time_buf[time_len] = '\0';

    char *msg = dd_load_text_ext(dd, FILENAME_MESSAGE, 0
                | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                | DD_FAIL_QUIETLY
    );
    const char *reported = (msg ? GTK_STOCK_YES : GTK_STOCK_NO);
    free(msg);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    char *hostname = dd_load_text(dd, FILENAME_HOSTNAME);

    GtkTreeIter iter;
    gtk_list_store_append(s_dumps_list_store, &iter);
    gtk_list_store_set(s_dumps_list_store, &iter,
                          COLUMN_REPORTED, reported,
                          COLUMN_APPLICATION, executable,
                          COLUMN_HOSTNAME, hostname,
                          //OPTION: time format
                          COLUMN_LATEST_CRASH_STR, time_buf,
                          COLUMN_LATEST_CRASH, (int)time,
                          COLUMN_DUMP_DIR, dirname,
                          -1);

    free(hostname);
    free(executable);

    dd_close(dd);
    VERB1 log("added: %s\n", dirname);
}


/* create_main_window and helpers */

static void on_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        //gtk_tree_model_get_iter(store, &iter, path);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            GValue d_dir = { 0 };
            gtk_tree_model_get_value(store, &iter, COLUMN_DUMP_DIR, &d_dir);

            pid_t pid = vfork();
            if (pid == 0) {
                execlp("bug-reporting-wizard", "bug-reporting-wizard", g_value_get_string(&d_dir), NULL);
                perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
            }
        }
    }
}

static gint on_key_press_event_cb(GtkTreeView *treeview, GdkEventKey *key, gpointer unused)
{
    int k = key->keyval;

    if (k == GDK_Delete || k == GDK_KP_Delete)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
        if (selection)
        {
            GtkTreeIter iter;
            GtkTreeModel *store = gtk_tree_view_get_model(treeview);
            if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
            {
		GtkTreePath *old_path = gtk_tree_model_get_path(store, &iter);

                GValue d_dir = { 0 };
                gtk_tree_model_get_value(store, &iter, COLUMN_DUMP_DIR, &d_dir);
                const char *dump_dir_name = g_value_get_string(&d_dir);

                g_print("CALL: del_event(%s)\n", dump_dir_name);

                /* Try to delete it ourselves */
                struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
                if (dd)
                {
                    if (dd->locked) /* it is not readonly */
                    {
                        dd_delete(dd);
                        goto deleted_ok;
                    }
                    dd_close(dd);
                }

                /* Ask abrtd to do it for us */
                if (connect_to_abrtd_and_call_DeleteDebugDump(dump_dir_name) == 0)
                {
 deleted_ok:
                    gtk_list_store_remove(s_dumps_list_store, &iter);
                }
                else
                {
                    /* Strange. Deletion did not succeed. Someone else deleted it?
                     * Rescan the whole list */
                    gtk_list_store_clear(s_dumps_list_store);
                    scan_dirs_and_add_to_dirlist();
                }

                /* Try to retain the same cursor position */
                sanitize_cursor(old_path);
                gtk_tree_path_free(old_path);
            }
        }

        return TRUE;
    }
    return FALSE;
}

static void add_columns(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* column reported */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes(_("Reported"),
                                                     renderer,
                                                     "stock_id",
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

GtkWidget *create_menu(void)
{
    /* main bar */
    GtkWidget *menu = gtk_menu_bar_new();
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic(_("_File"));
    GtkWidget *edit_item = gtk_menu_item_new_with_mnemonic(_("_Edit"));
    GtkWidget *help_item = gtk_menu_item_new_with_mnemonic(_("_Help"));

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), file_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), edit_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), help_item);

    /* file submenu */
    GtkWidget *file_submenu = gtk_menu_new();
    GtkWidget *quit_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_submenu), quit_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_submenu);

    /* edit submenu */
    GtkWidget *edit_submenu = gtk_menu_new();
    GtkWidget *plugins_item = gtk_menu_item_new_with_mnemonic(_("_Plugins"));
    GtkWidget *preferences_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), plugins_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), preferences_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_submenu);

    /* help submenu */
    GtkWidget *help_submenu = gtk_menu_new();
    GtkWidget *log_item = gtk_menu_item_new_with_mnemonic(_("View _log"));
    GtkWidget *online_help_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
    GtkWidget *about_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), log_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), online_help_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), about_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_submenu);

    return menu;
}

GtkWidget *create_main_window(void)
{
    /* main window */
    GtkWidget *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 600, 700);
    gtk_window_set_title(GTK_WINDOW(main_window), _("Automatic Bug Reporting Tool"));
    gtk_window_set_icon_name(GTK_WINDOW(main_window), "abrt");


    GtkWidget *main_vbox = gtk_vbox_new(false, 0);

    /* scrolled region inside main window */
    GtkWidget *scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_win),
                                          GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win),
                                          GTK_POLICY_AUTOMATIC,
                                          GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(GTK_BOX(main_vbox), create_menu(), false, false, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), scroll_win, true, true, 0);
    gtk_container_add(GTK_CONTAINER(main_window), main_vbox);

    /* tree view inside scrolled region */
    s_treeview = gtk_tree_view_new();
    add_columns(GTK_TREE_VIEW(s_treeview));
    gtk_container_add(GTK_CONTAINER(scroll_win), s_treeview);

    /* Create data store for the list and attach it */
    s_dumps_list_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, /* reported */
                                                       G_TYPE_STRING, /* executable */
                                                       G_TYPE_STRING, /* hostname */
                                                       G_TYPE_STRING, /* time */
                                                       G_TYPE_INT,    /* unix time - used for sort */
                                                       G_TYPE_STRING);/* dump dir path */
    gtk_tree_view_set_model(GTK_TREE_VIEW(s_treeview), GTK_TREE_MODEL(s_dumps_list_store));

    /* Double click/Enter handler */
    g_signal_connect(s_treeview, "row-activated", G_CALLBACK(on_row_activated_cb), NULL);
    /* Delete handler */
    g_signal_connect(s_treeview, "key-press-event", G_CALLBACK(on_key_press_event_cb), NULL);
    /* Quit when user closes the main window */
    g_signal_connect(main_window, "destroy", gtk_main_quit, NULL);

    return main_window;
}

void sanitize_cursor(GtkTreePath *preferred_path)
{
    GtkTreePath *path;

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(s_treeview), &path, /* GtkTreeViewColumn** */ NULL);
    if (path)
    {
        /* Cursor exists already */
        goto ret;
    }

    if (preferred_path)
    {
        /* Try to position cursor on preferred_path */
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(s_treeview), preferred_path,
                /* GtkTreeViewColumn *focus_column */ NULL, /* start_editing */ false);

        /* Did it work? */
        gtk_tree_view_get_cursor(GTK_TREE_VIEW(s_treeview), &path, /* GtkTreeViewColumn** */ NULL);
        if (path) /* yes */
        {
            goto ret;
        }
    }

    /* Try to position cursor on 1st element */
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(s_dumps_list_store), &iter))
    {
        /* We have at least one element, put cursor on it */

        /* Get path from iter pointing to 1st element */
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(s_dumps_list_store), &iter);

        /* Use it to set cursor */
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(s_treeview), path,
                /* GtkTreeViewColumn *focus_column */ NULL, /* start_editing */ false);
    }
    /* else we have no elements */

 ret:
    gtk_tree_path_free(path);

    /* Without this, the *header* of the list gets the focus. Ugly. */
    gtk_widget_grab_focus(s_treeview);
}
