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
#include <gdk/gdkkeysyms.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "abrt-gtk.h"
#include "libreport-gtk.h"

static const char help_uri[] = "http://docs.fedoraproject.org/en-US/"
    "Fedora/14/html/Deployment_Guide/ch-abrt.html";

static GtkListStore *s_dumps_list_store;
static GtkWidget *s_treeview;
static GtkWidget *g_main_window;

enum
{
    COLUMN_REPORTED,
    COLUMN_REASON,
    COLUMN_DIRNAME,
    COLUMN_LATEST_CRASH_STR,
    COLUMN_LATEST_CRASH,
    COLUMN_DUMP_DIR,
    COLUMN_BG,
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

    char *msg = dd_load_text_ext(dd, FILENAME_REPORTED_TO, 0
                | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                | DD_FAIL_QUIETLY_ENOENT
                | DD_FAIL_QUIETLY_EACCES
    );
    const char *reported = (msg ? GTK_STOCK_YES : GTK_STOCK_NO);
    free(msg);
    char *reason = dd_load_text(dd, FILENAME_REASON);

    static bool grey_bg = false;

    GtkTreeIter iter;
    gtk_list_store_append(s_dumps_list_store, &iter);
    gtk_list_store_set(s_dumps_list_store, &iter,
                          COLUMN_REPORTED, reported,
                          COLUMN_REASON, reason,
                          COLUMN_DIRNAME, dd->dd_dirname,
                          //OPTION: time format
                          COLUMN_LATEST_CRASH_STR, time_buf,
                          COLUMN_LATEST_CRASH, (int)time,
                          COLUMN_DUMP_DIR, dirname,
                          COLUMN_BG, grey_bg ? "#EEEEEE" : "#FFFFFF",
                          -1);
    grey_bg = !grey_bg;
    free(reason);

    dd_close(dd);
    VERB1 log("added: %s", dirname);
}

void rescan_dirs_and_add_to_dirlist(void)
{
    gtk_list_store_clear(s_dumps_list_store);
    scan_dirs_and_add_to_dirlist();
}


/* create_main_window and helpers */

static void on_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            GValue d_dir = { 0 };
            gtk_tree_model_get_value(store, &iter, COLUMN_DUMP_DIR, &d_dir);

            pid_t pid = vfork();
            if (pid == 0)
            {
                /* Undo signal(SIGCHLD, SIG_IGN), or child inherits it and gets terribly confused */
                /*signal(SIGCHLD, SIG_DFL); - not needed, we dont set it to SIG_IGN in main anymore */

                const char *dirname= g_value_get_string(&d_dir);
                VERB1 log("Executing: %s %s", "bug-reporting-wizard", dirname);
                execlp("bug-reporting-wizard", "bug-reporting-wizard", dirname, NULL);
                perror_msg_and_die("Can't execute %s", "bug-reporting-wizard");
            }
        }
    }
}

static void on_btn_report_cb(GtkButton *button, gpointer user_data)
{
    on_row_activated_cb(GTK_TREE_VIEW(s_treeview), NULL, NULL, NULL);
}

static void delete_report(GtkTreeView *treeview)
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

            VERB1 log("Deleting '%s'", dump_dir_name);
            if (delete_dump_dir_possibly_using_abrtd(dump_dir_name) == 0)
            {
                gtk_list_store_remove(s_dumps_list_store, &iter);
            }
            else
            {
                /* Strange. Deletion did not succeed. Someone else deleted it?
                 * Rescan the whole list */
                rescan_dirs_and_add_to_dirlist();
            }

            /* Try to retain the same cursor position */
            sanitize_cursor(old_path);
            gtk_tree_path_free(old_path);
        }
    }
}

static gint on_key_press_event_cb(GtkTreeView *treeview, GdkEventKey *key, gpointer unused)
{
    int k = key->keyval;

    if (k == GDK_Delete || k == GDK_KP_Delete)
    {
        delete_report(treeview);
        return TRUE;
    }
    return FALSE;
}

static void on_btn_delete_cb(GtkButton *button, gpointer unused)
{
    delete_report(GTK_TREE_VIEW(s_treeview));
}

static void on_btn_online_help_cb(GtkButton *button, gpointer unused)
{
    gtk_show_uri(NULL, help_uri, GDK_CURRENT_TIME, NULL);
}

static void on_menu_help_cb(GtkMenuItem *menuitem, gpointer unused)
{
    gtk_show_uri(NULL, help_uri, GDK_CURRENT_TIME, NULL);
}

static void on_menu_about_cb(GtkMenuItem *menuitem, gpointer unused)
{
    static const char copyright_str[] = "Copyright © 2009, 2010, 2011 Red Hat, Inc";

    static const char license_str[] = "This program is free software; you can redistribut"
        "e it and/or modify it under the terms of the GNU General Public License "
        "as published by the Free Software Foundation; either version 2 of the Li"
        "cense, or (at your option) any later version.\n\nThis program is distrib"
        "uted in the hope that it will be useful, but WITHOUT ANY WARRANTY; witho"
        "ut even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICU"
        "LAR PURPOSE.  See the GNU General Public License for more details.\n\nYo"
        "u should have received a copy of the GNU General Public License along wi"
        "th this program.  If not, see <http://www.gnu.org/licenses/>.";

    static const char website_url[] = "https://fedorahosted.org/abrt/";

    static const char *authors[] = {
        "Anton Arapov <aarapov@redhat.com>",
        "Karel Klic <kklic@redhat.com>",
        "Jiri Moskovcak <jmoskovc@redhat.com>",
        "Nikola Pajkovsky <npajkovs@redhat.com>",
        "Denys Vlasenko <dvlasenk@redhat.com>",
        "Michal Toman <mtoman@redhat.com>",
        "Zdenek Prikryl",
        NULL
    };

    static const char *artists[] = {
        "Patrick Connelly <pcon@fedoraproject.org>",
        "Máirín Duffy <duffy@fedoraproject.org>",
        "Lapo Calamandrei",
        "Jakub Steinar <jsteiner@redhat.com>",
        NULL
    };

    GtkWidget *about_d = gtk_about_dialog_new();

    gtk_window_set_icon_name(GTK_WINDOW(about_d), "abrt");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_d), VERSION);
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_d), "abrt");
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_d), "ABRT");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_d), copyright_str);
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_d), license_str);
    gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(about_d),true);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_d), website_url);
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_d), authors);
    gtk_about_dialog_set_artists(GTK_ABOUT_DIALOG(about_d), artists);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about_d), _("translator-credits"));

    gtk_window_set_transient_for(GTK_WINDOW(about_d), GTK_WINDOW(g_main_window));

    gtk_dialog_run(GTK_DIALOG(about_d));
    gtk_widget_hide(GTK_WIDGET(about_d));
}

static void show_events_list_dialog_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    show_events_list_dialog(GTK_WINDOW(g_main_window));
}

static void add_columns(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes(_("Reported"),
                                                     renderer,
                                                     "stock_id",
                                                     COLUMN_REPORTED,
                                                     "cell-background",
                                                     COLUMN_BG,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_REPORTED);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Problem"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_REASON,
                                                     "cell-background",
                                                     COLUMN_BG,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_REASON);
    gtk_tree_view_append_column(treeview, column);

    /*
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Stored in"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_DIRNAME,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_DIRNAME);
    gtk_tree_view_append_column(treeview, column);
    */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Last occurrence"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_LATEST_CRASH_STR,
                                                     "cell-background",
                                                     COLUMN_BG,
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

    g_signal_connect(quit_item, "activate", &gtk_main_quit, NULL);

    /* edit submenu */
    GtkWidget *edit_submenu = gtk_menu_new();
    GtkWidget *plugins_item = gtk_menu_item_new_with_mnemonic(_("_Event configuration"));
    //GtkWidget *preferences_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), plugins_item);
    //gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), preferences_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_submenu);

    g_signal_connect(plugins_item, "activate", G_CALLBACK(show_events_list_dialog_cb), NULL);


    /* help submenu */
    GtkWidget *help_submenu = gtk_menu_new();
    GtkWidget *online_help_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
    GtkWidget *about_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), online_help_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), about_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_submenu);

    g_signal_connect(online_help_item, "activate", G_CALLBACK(on_menu_help_cb), NULL);
    g_signal_connect(about_item, "activate", G_CALLBACK(on_menu_about_cb), NULL);

    return menu;
}

GtkWidget *create_main_window(void)
{
    /* main window */
    g_main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(g_main_window), 700, 700);
    gtk_window_set_title(GTK_WINDOW(g_main_window), _("Automatic Bug Reporting Tool"));
    gtk_window_set_default_icon_name("abrt");

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
    gtk_container_add(GTK_CONTAINER(g_main_window), main_vbox);

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
                                                       G_TYPE_STRING, /* dump dir path */
                                                       G_TYPE_STRING);/* row background */

    gtk_tree_view_set_model(GTK_TREE_VIEW(s_treeview), GTK_TREE_MODEL(s_dumps_list_store));

    /* buttons are homogenous so set size only for one button and it will
     * work for the rest buttons in same gtk_hbox_new() */
    GtkWidget *btn_report = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_widget_set_size_request(btn_report, 200, 30);

    GtkWidget *btn_delete = gtk_button_new_from_stock(GTK_STOCK_DELETE);

    GtkWidget *hbox_report_delete = gtk_hbox_new(true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_report_delete), btn_delete, true, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox_report_delete), btn_report, true, true, 0);

    GtkWidget *halign =  gtk_alignment_new(1, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(halign), hbox_report_delete);

    GtkWidget *hbox_help_close = gtk_hbutton_box_new();
    GtkWidget *btn_online_help = gtk_button_new_with_mnemonic(_("Online _Help"));
    GtkWidget *btn_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_box_pack_end(GTK_BOX(hbox_help_close), btn_online_help, false, false, 0);
    gtk_box_pack_end(GTK_BOX(hbox_help_close), btn_close, false, false, 0);

    gtk_box_pack_start(GTK_BOX(main_vbox), halign, false, false, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox_help_close, false, false, 10);

    /* Double click/Enter handler */
    g_signal_connect(s_treeview, "row-activated", G_CALLBACK(on_row_activated_cb), NULL);
    g_signal_connect(btn_report, "clicked", G_CALLBACK(on_btn_report_cb), NULL);
    /* Delete handler */
    g_signal_connect(s_treeview, "key-press-event", G_CALLBACK(on_key_press_event_cb), NULL);
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_btn_delete_cb), NULL);
    /* Quit when user closes the main window */
    g_signal_connect(g_main_window, "destroy", gtk_main_quit, NULL);
    /* Quit when user click on Cancel button */
    g_signal_connect(btn_close, "clicked", gtk_main_quit, NULL);
    /* Show online help */
    g_signal_connect(btn_online_help, "clicked", G_CALLBACK(on_btn_online_help_cb), NULL);
    return g_main_window;
}

GtkTreePath *get_cursor(void)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(s_treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            return gtk_tree_model_get_path(store, &iter);
        }
    }
    return NULL;
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
