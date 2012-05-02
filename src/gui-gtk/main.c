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
#include <sys/inotify.h>
#include <gio/gio.h> //dbus
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include <internal_libreport_gtk.h>
#include "libabrt.h"
#include "abrt-dbus.h"

#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 22
# define GDK_KEY_Delete GDK_Delete
# define GDK_KEY_KP_Delete GDK_KP_Delete
#endif

static void scan_dirs_and_add_to_dirlist(void);
static void rescan_and_refresh(void);


static const char help_uri[] = "http://docs.fedoraproject.org/en-US/"
    "Fedora/14/html/Deployment_Guide/ch-abrt.html";

static GtkListStore *s_dumps_list_store;
static GtkListStore *s_reported_dumps_list_store;

static GtkWidget *s_treeview;
static GtkWidget *s_reported_treeview;
static GtkWidget *s_active_treeview;
static GtkTreePath *s_path_treeview;
static GtkTreePath *s_path_reported_treeview;

static GtkWidget *g_main_window;
static GtkWidget *s_report_window;

enum
{
    COLUMN_SOURCE,
    COLUMN_REASON,
    COLUMN_LATEST_CRASH_STR,
    COLUMN_LATEST_CRASH,
    COLUMN_DUMP_DIR,
    COLUMN_REPORTED_TO,
    NUM_COLUMNS
};

static gint g_authorize; /* wheter to try to authorize when getting list of problems */

//FIXME: maybe we can use strrchr and make this faster...
static char *get_last_line(const char* msg)
{
    const char *curr_end = NULL;
    const char *start = msg;
    const char *end = msg;

    while((curr_end = strchr(end, '\n')) != NULL)
    {
        end = curr_end;
        curr_end = strchr(end+1, '\n');
        if (curr_end == NULL || strchr(end+2, '\n') == NULL)
            break;

        start = end+1;
        end = curr_end;
    }

    //fix the case where reported_to has only 1 line without \n
    if (end == msg)
        end = end + strlen(msg);

    return xstrndup(start, end - start);
}

static void show_warning_dialog(const char* message, GtkWidget *parent)
{
    if (!parent)
        parent = g_main_window;

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_WARNING,
                                        GTK_BUTTONS_OK,
                                        message
                                        );
    g_signal_connect_swapped(dialog,
                             "response",
                             G_CALLBACK(gtk_widget_destroy),
                             dialog);


    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static problem_data_t* get_problem_data_dbus(const char *problem_dir_path, GError *error)
{
    problem_data_t *pd;
    char *key, *val;

    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     ABRT_DBUS_NAME,
                                                     ABRT_DBUS_OBJECT,
                                                     ABRT_DBUS_IFACE,
                                                     NULL,
                                                     &error);

    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                            "GetInfo",
                                            g_variant_new("(s)", problem_dir_path),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);

    pd = new_problem_data();

    GVariantIter *iter;
    g_variant_get(result, "(a{ss})", &iter);
    while (g_variant_iter_loop(iter, "{ss}", &key, &val))
    {
        add_to_problem_data(pd, key, val);
    }
    g_variant_unref(result);
    return pd;
}

static void add_directory_to_dirlist(const char *problem_dir_path, gpointer data)
{
    /* Silently ignore *any* errors, not only EACCES.
     * We saw "lock file is locked by process PID" error
     * when we raced with wizard.
     */
    int sv_logmode = logmode;
    logmode = 0;
    logmode = sv_logmode;

    problem_data_t *pd = get_problem_data_dbus(problem_dir_path, NULL);
    char time_buf[sizeof("YYYY-MM-DD hh:mm:ss")];
    time_buf[0] = '\0';
    const char *time_str = get_problem_item_content_or_NULL(pd, FILENAME_TIME);
    time_t t = 0;
    if (time_str && time_str[0])
    {
        t = strtol(time_str, NULL, 10); /* atoi won't work past 2038! */
        struct tm *ptm = localtime(&t);
        size_t time_len = strftime(time_buf, sizeof(time_buf)-1, "%Y-%m-%d %H:%M", ptm);
        time_buf[time_len] = '\0';
    }

    const char *not_reportable_reason = get_problem_item_content_or_NULL(pd, FILENAME_NOT_REPORTABLE);
    const char *reason = get_problem_item_content_or_NULL(pd, FILENAME_REASON);

    /* the source of the problem:
     * - first we try to load component, as we use it on Fedora
    */
    const char *source = get_problem_item_content_or_NULL(pd, "source");

    const char *msg = get_problem_item_content_or_NULL(pd, FILENAME_REPORTED_TO);


    GtkListStore *list_store;

    char *subm_status = NULL;
    if (msg)
    {
        list_store = s_reported_dumps_list_store;
        subm_status = get_last_line(msg);
    }
    else
        list_store = s_dumps_list_store;

    GtkTreeIter iter;
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter,
                          COLUMN_SOURCE, source,
                          COLUMN_REASON, not_reportable_reason? :reason,
                          //OPTION: time format
                          COLUMN_LATEST_CRASH_STR, time_buf,
                          COLUMN_LATEST_CRASH, t,
                          COLUMN_DUMP_DIR, problem_dir_path,
                          COLUMN_REPORTED_TO, msg ? subm_status : NULL,
                          -1);

    free_problem_data(pd);

    VERB1 log("added: %s", problem_dir_path);
}

GList *get_problems_over_dbus(const char *dump_location, GError **error)
{
    GDBusProxy * proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             NULL,
                                             ABRT_DBUS_NAME,
                                             ABRT_DBUS_OBJECT,
                                             ABRT_DBUS_IFACE,
                                             NULL,
                                             error);

    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                    g_authorize ? "GetAllProblems" : "GetProblems",
                                     g_variant_new("(s)", dump_location),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     error);
    GList *list = string_list_from_variant(result);
    return list;
}

static void rescan_dirs_and_add_to_dirlist(void)
{
    gtk_list_store_clear(s_dumps_list_store);
    gtk_list_store_clear(s_reported_dumps_list_store);
    scan_dirs_and_add_to_dirlist();
}


static GtkTreePath *get_cursor(void)
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

static void sanitize_cursor(GtkTreePath *preferred_path)
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
            goto ret;
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


/* create_main_window and helpers */

static void deactivate_selection(GtkTreeView *tv)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tv);
    if (selection)
    {
        /* Remember the selection */
        GList *list = gtk_tree_selection_get_selected_rows(selection, /*GtkTreeModel **model:*/ NULL);
        /* Note: returned list is a copy! We take ownership (IOW: must free it) */
        GtkTreePath **pp_path = NULL;
        if ((void*)tv == (void*)s_treeview)
            pp_path = &s_path_treeview;
        if ((void*)tv == (void*)s_reported_treeview)
            pp_path = &s_path_reported_treeview;
        if (pp_path && list)
        {
            if (*pp_path)
                gtk_tree_path_free(*pp_path);
            *pp_path = list->data;
            list = g_list_delete_link(list, list);
        }
        g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
        g_list_free(list);
        /* Deactivate */
        gtk_tree_selection_unselect_all(selection);
    }
}

/* Contrary to what I expected, this callback is NOT called
 * when I select a row with a mouse.
 * Maybe this callback can be removed.
 */
static gboolean on_select_cursor_row_cb(
                GtkTreeView *treeview,
                gboolean     arg1,
                gpointer     user_data)
{
    //log("SELECT_ROW:%p/%p", treeview, user_data);
    deactivate_selection(GTK_TREE_VIEW(user_data));
    return false; /* propagate the event further */
}

/* Called when I select a row with a mouse (among other cases).
 * Make sure other tv loses its selection.
 */
static gboolean on_cursor_changed_cb(
                GtkTreeView *treeview,
                gpointer     user_data)
{
    //log("cursor-changed:%p/%p", treeview, user_data);
    deactivate_selection(GTK_TREE_VIEW(user_data));

    /* Without this, only <tab>ing to treeview sets s_active_treeview.
     * Thus, selecting a row with mouse and clicking [Open] with mouse
     * wouldn't work!
     */
    s_active_treeview = GTK_WIDGET(treeview);

    return false; /* propagate the event further */
}

/* Called when I <tab> to the treeview
 * Make sure other tv loses its selection.
 * Also, restore selection in current tv if it was desecleted
 * (for example, if we <tab>ed to the other seletion a few sec ago...).
 */
static gboolean on_focus_cb(
                GtkWidget *widget,
                GtkDirectionType direction,
                gpointer user_data)
{
    //log("FOCUS:%p/%p", widget, user_data);
    s_active_treeview = widget;

    /* Deactivate the other one */
    deactivate_selection(GTK_TREE_VIEW(user_data));

    /* Re-activate row in this one if it was deactivated */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    if (selection && gtk_tree_selection_count_selected_rows(selection) < 1)
    {
        GtkTreePath **pp_path = NULL;
        if (widget == s_treeview)
            pp_path = &s_path_treeview;
        if (widget == s_reported_treeview)
            pp_path = &s_path_reported_treeview;
        if (pp_path && *pp_path)
        {
            gtk_tree_selection_select_path(selection, *pp_path);
            gtk_tree_path_free(*pp_path);
            *pp_path = NULL;
        }
    }

    return false; /* propagate the event further */
}

static int chown_dir_over_dbus(const char *problem_dir_path)
{
    GError *error = NULL;
    GDBusProxy * proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         ABRT_DBUS_NAME,
                                         ABRT_DBUS_OBJECT,
                                         ABRT_DBUS_IFACE,
                                         NULL,
                                         &error);

    g_dbus_proxy_call_sync(proxy,
                        "ChownProblemDir",
                        g_variant_new("(s)", problem_dir_path),
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        NULL,
                        &error);

    if (error)
    {
        //TODO show a warning dialog here or on the higher level?
        error_msg(_("Can't chown '%s': %s"),problem_dir_path, error->message);
        g_error_free(error);
        return 1;
    }
    return 0;
}

static int delete_problem_dirs_over_dbus(GList *problem_dir_paths)
{
    GError *error = NULL;

    GVariant *parameters = variant_from_string_list(problem_dir_paths);

    GDBusProxy * proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         ABRT_DBUS_NAME,
                                         ABRT_DBUS_OBJECT,
                                         ABRT_DBUS_IFACE,
                                         NULL,
                                         &error);

    g_dbus_proxy_call_sync(proxy,
                    "DeleteProblem",
                    parameters,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    NULL,
                    &error);

    if (error)
    {
        //TODO show a warning dialog here or on the higher level?
        error_msg(_("Deleting problem directory failed: %s"), error->message);
        g_error_free(error);
        return 1;
    }
    return 0;
}

static const char *current_dirname(GtkTreeView *treeview)
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
            const char *dirname = g_value_get_string(&d_dir);
            return dirname;
        }
    }
    return NULL;
}

static void on_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    const char *dirname = current_dirname(treeview);
    if (dirname)
    {
        if (chown_dir_over_dbus(dirname) == 0)
        {
            report_problem_in_dir(dirname,
                              LIBREPORT_ANALYZE | LIBREPORT_NOWAIT | LIBREPORT_GETPID);
        }
        //else
        // TODO: show a warning dialog
    }
}

static void open_problem_data_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    const char *dirname = current_dirname(GTK_TREE_VIEW(s_active_treeview));
    if (dirname)
    {
        if (chown_dir_over_dbus(dirname) != 0)
        {
            // TODO: show a warning dialog
            error_msg("Can't chown '%s'", dirname);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) /* error */
            perror_msg_and_die("fork");
        if (pid == 0) /* child */
        {
            struct run_event_state *run_state = new_run_event_state();
            int r = run_event_on_dir_name(run_state, dirname, "open-gui");
            int no_such_event = (r == 0 && run_state->children_count == 0);
            free_run_event_state(run_state);
            if (!no_such_event)
            {
                exit(r);
            }
            /* Default: launch graphical tool */
            execl(BIN_DIR"/report-gtk", "report-gtk", "--", (char *)dirname, NULL);
            execlp("report-gtk", "report-gtk", "--", (char *)dirname, NULL);
            perror_msg_and_die("Can't execute %s", "report-gtk");
        }
    }
}

static void on_btn_report_cb(GtkButton *button, gpointer user_data)
{
    if (s_active_treeview)
        on_row_activated_cb(GTK_TREE_VIEW(s_active_treeview), NULL, NULL, NULL);
}

static void on_column_change_cb(GtkTreeSortable *sortable, gpointer user_data)
{
    const char *name;
    char *value;
    GtkSortType sort_type;
    int col_id;

    if (sortable == GTK_TREE_SORTABLE(s_dumps_list_store))
        name = "sort_column";
    else if (sortable == GTK_TREE_SORTABLE(s_reported_dumps_list_store))
        name = "sort_reported_column";
    else
        assert(0);

    gtk_tree_sortable_get_sort_column_id(sortable, &col_id, &sort_type);
    if (sort_type == GTK_SORT_ASCENDING)
        col_id = -col_id - 1; /* store ascending sort as negative id */
    value = xasprintf("%d", col_id);
    set_user_setting(name, value);
    free(value);
}

static void on_show_all_cb(GtkToggleButton *togglebutton, gpointer data)
{
    g_authorize = gtk_toggle_button_get_active(togglebutton);
    rescan_and_refresh();
}

static void load_sort_setting(GtkTreeSortable *sortable)
{
    const char *name, *value;
    GtkSortType sort_type;
    int col_id;

    if (sortable == GTK_TREE_SORTABLE(s_dumps_list_store))
        name = "sort_column";
    else if (sortable == GTK_TREE_SORTABLE(s_reported_dumps_list_store))
        name = "sort_reported_column";
    else
        return;

    /* defaults */
    col_id = COLUMN_LATEST_CRASH;
    sort_type = GTK_SORT_DESCENDING;

    value = get_user_setting(name);
    if (value)
    {
        col_id = xatoi(value);
        if (col_id < 0)
        {
            /* negative id means ascending order */
            col_id = -col_id - 1;
            sort_type = GTK_SORT_ASCENDING;
        }
    }

    if (col_id >= NUM_COLUMNS)
        col_id = COLUMN_LATEST_CRASH;

    gtk_tree_sortable_set_sort_column_id(sortable, col_id, sort_type);
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

            //TODO: I plan to implement deleting multiple items at once rhbz#541928
            GList *problem_dir_paths = NULL;
            problem_dir_paths = g_list_append(problem_dir_paths, xstrdup(dump_dir_name));

            if (delete_problem_dirs_over_dbus(problem_dir_paths) != 0)
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

    if (k == GDK_KEY_Delete || k == GDK_KEY_KP_Delete)
    {
        delete_report(treeview);
        return TRUE;
    }
    return FALSE;
}

static void on_btn_delete_cb(GtkButton *button, gpointer unused)
{
    if (s_active_treeview)
        delete_report(GTK_TREE_VIEW(s_active_treeview));
}

static void on_menu_help_cb(GtkMenuItem *menuitem, gpointer unused)
{
    gtk_show_uri(NULL, help_uri, GDK_CURRENT_TIME, NULL);
}

static void on_button_send_cb(GtkWidget *button, gpointer data)
{
    GtkTextView *tev = GTK_TEXT_VIEW(data);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(tev);
    GtkTextIter it_start;
    GtkTextIter it_end;
    gtk_text_buffer_get_start_iter(buf, &it_start);
    gtk_text_buffer_get_end_iter(buf, &it_end);
    gchar *text = gtk_text_buffer_get_text(buf,
                                           &it_start,
                                           &it_end,
                                           false);

    problem_data_t *pd = new_problem_data();

    if (text[0])
    {
        add_to_problem_data(pd, "description", text);
    }

    /* why it doesn't want to hide before report ends? */
    gtk_widget_destroy(s_report_window);

    report_problem_in_memory(pd, LIBREPORT_NOWAIT | LIBREPORT_GETPID);
    free_problem_data(pd);
}

static void on_menu_report_cb(GtkMenuItem *menuitem, gpointer unused)
{
    s_report_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(s_report_window), _("Problem description"));
    gtk_window_set_default_size(GTK_WINDOW(s_report_window), 400, 400);
    GtkWidget *vbox = gtk_vbox_new(false, 0);
    GtkWidget *button_send = gtk_button_new_with_label(_("Send"));
    GtkWidget *tev = gtk_text_view_new();
    g_signal_connect(button_send, "clicked", G_CALLBACK(on_button_send_cb), tev);

    gtk_box_pack_start(GTK_BOX(vbox), tev, true, true, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button_send, false, false, 5);

    gtk_container_add(GTK_CONTAINER(s_report_window), vbox);

    gtk_widget_show_all(s_report_window);
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

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Source"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_SOURCE,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_SOURCE);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Problem"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_REASON,
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
    column = gtk_tree_view_column_new_with_attributes(_("Last Occurrence"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_LATEST_CRASH_STR,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_LATEST_CRASH);
    gtk_tree_view_append_column(treeview, column);
}

static void add_columns_reported(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Source"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_SOURCE,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_SOURCE);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Problem"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_REASON,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_REASON);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Date Submitted"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_LATEST_CRASH_STR,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_LATEST_CRASH);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Submission Result"),
                                                     renderer,
                                                     "text",
                                                     COLUMN_REPORTED_TO,
                                                     NULL);
    //gtk_tree_view_column_set_sort_column_id(column, COLUMN_LATEST_CRASH);
    gtk_tree_view_append_column(treeview, column);
}

static GtkWidget *create_menu(void)
{
    /* main bar */
    GtkWidget *menu = gtk_menu_bar_new();
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic(_("_Report"));
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
    GtkWidget *open_problem_item = gtk_menu_item_new_with_label(_("Open problem data"));
    GtkWidget *preferences_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), open_problem_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), preferences_item);
    //gtk_menu_shell_append(GTK_MENU_SHELL(edit_submenu), preferences_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_submenu);

    g_signal_connect(open_problem_item, "activate", G_CALLBACK(open_problem_data_cb), NULL);
    g_signal_connect(preferences_item, "activate", G_CALLBACK(show_events_list_dialog_cb), NULL);

    /* help submenu */
    GtkWidget *help_submenu = gtk_menu_new();
    GtkWidget *online_help_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
    GtkWidget *report_problem_item = gtk_menu_item_new_with_label(_("Report problem with ABRT"));
    GtkWidget *about_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), online_help_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), report_problem_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_submenu), about_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_submenu);

    g_signal_connect(online_help_item, "activate", G_CALLBACK(on_menu_help_cb), NULL);
    g_signal_connect(report_problem_item, "activate", G_CALLBACK(on_menu_report_cb), NULL);
    g_signal_connect(about_item, "activate", G_CALLBACK(on_menu_about_cb), NULL);

    return menu;
}

static GtkWidget *create_main_window(void)
{
    /* Main window */
    g_main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(g_main_window), 700, 700);
    gtk_window_set_title(GTK_WINDOW(g_main_window), _("Automatic Bug Reporting Tool"));
    gtk_window_set_default_icon_name("abrt");

    GtkWidget *main_vbox = gtk_vbox_new(false, 0);
    /* add menu */
    gtk_box_pack_start(GTK_BOX(main_vbox), create_menu(), false, false, 0);

    GtkWidget *not_subm_vbox = gtk_vbox_new(false, 0);
    gtk_container_set_border_width(GTK_CONTAINER(not_subm_vbox), 10);
    GtkWidget *subm_vbox = gtk_vbox_new(false, 0);
    gtk_container_set_border_width(GTK_CONTAINER(subm_vbox), 10);

    /* Scrolled region for not reported problems inside main window*/
    GtkWidget *new_problems_scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(new_problems_scroll_win),
                                          GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(new_problems_scroll_win),
                                          GTK_POLICY_AUTOMATIC,
                                          GTK_POLICY_AUTOMATIC);

    GtkWidget *not_subm_lbl = gtk_label_new(_("Not submitted reports"));
    gtk_misc_set_alignment(GTK_MISC(not_subm_lbl), 0, 0);
    gtk_label_set_markup(GTK_LABEL(not_subm_lbl), _("<b>Not submitted reports</b>"));

    GtkWidget *show_all_btn = gtk_check_button_new_with_label(_("Show all problems"));
    g_signal_connect(show_all_btn, "toggled", G_CALLBACK(on_show_all_cb), NULL);

    /* add label for not submitted tree view */
    gtk_box_pack_start(GTK_BOX(not_subm_vbox), not_subm_lbl, false, false, 0);
    gtk_box_pack_start(GTK_BOX(not_subm_vbox), new_problems_scroll_win, true, true, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), not_subm_vbox, true, true, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), show_all_btn, false, false, 0);

    /* Two tree views */
    s_treeview = gtk_tree_view_new();
    s_reported_treeview = gtk_tree_view_new();

    /* Tree view inside scrolled region */
    g_object_set(s_treeview, "rules-hint", 1, NULL); /* use alternating colors */
    add_columns(GTK_TREE_VIEW(s_treeview));
    gtk_container_add(GTK_CONTAINER(new_problems_scroll_win), s_treeview);

    /* Create data store for the list and attach it */
    s_dumps_list_store = gtk_list_store_new(NUM_COLUMNS,
                                           G_TYPE_STRING, /* source */
                                           G_TYPE_STRING, /* executable */
                                           G_TYPE_STRING, /* time */
                                           G_TYPE_INT,    /* unix time - used for sort */
                                           G_TYPE_STRING, /* dump dir path */
                                           G_TYPE_STRING); /* reported_to */


    load_sort_setting(GTK_TREE_SORTABLE(s_dumps_list_store));

    g_signal_connect(GTK_TREE_SORTABLE(s_dumps_list_store), "sort-column-changed", G_CALLBACK(on_column_change_cb), NULL);

    gtk_tree_view_set_model(GTK_TREE_VIEW(s_treeview), GTK_TREE_MODEL(s_dumps_list_store));

    /* Double click/Enter handler */
    g_signal_connect(s_treeview, "row-activated", G_CALLBACK(on_row_activated_cb), NULL);
    g_signal_connect(s_treeview, "focus", G_CALLBACK(on_focus_cb), s_reported_treeview);
    g_signal_connect(s_treeview, "select-cursor-row", G_CALLBACK(on_select_cursor_row_cb), s_reported_treeview);
    g_signal_connect(s_treeview, "cursor-changed", G_CALLBACK(on_cursor_changed_cb), s_reported_treeview);
    /* Delete handler */
    g_signal_connect(s_treeview, "key-press-event", G_CALLBACK(on_key_press_event_cb), NULL);

    /* scrolled region for reported problems */
    GtkWidget *reported_problems_scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(reported_problems_scroll_win),
                                          GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(reported_problems_scroll_win),
                                          GTK_POLICY_AUTOMATIC,
                                          GTK_POLICY_AUTOMATIC);

    GtkWidget *subm_lbl = gtk_label_new(_("Submitted reports"));
    /* align to left */
    gtk_misc_set_alignment(GTK_MISC(subm_lbl), 0, 0);
    gtk_label_set_markup(GTK_LABEL(subm_lbl), _("<b>Submitted reports</b>"));

    /* add label for submitted tree view */
    gtk_box_pack_start(GTK_BOX(subm_vbox), subm_lbl, false, false, 0);
    gtk_box_pack_start(GTK_BOX(subm_vbox), reported_problems_scroll_win, true, true, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), subm_vbox, true, true, 0);

    /* Tree view inside scrolled region */
    g_object_set(s_reported_treeview, "rules-hint", 1, NULL); /* use alternating colors */
    add_columns_reported(GTK_TREE_VIEW(s_reported_treeview));
    gtk_container_add(GTK_CONTAINER(reported_problems_scroll_win), s_reported_treeview);

    /* Create data store for the list and attach it */
    s_reported_dumps_list_store = gtk_list_store_new(NUM_COLUMNS,
                                                       G_TYPE_STRING, /* source */
                                                       G_TYPE_STRING, /* executable */
                                                       G_TYPE_STRING, /* time */
                                                       G_TYPE_INT,    /* unix time - used for sort */
                                                       G_TYPE_STRING, /* dump dir path */
                                                       G_TYPE_STRING); /* reported_to */


    load_sort_setting(GTK_TREE_SORTABLE(s_reported_dumps_list_store));

    g_signal_connect(GTK_TREE_SORTABLE(s_reported_dumps_list_store), "sort-column-changed", G_CALLBACK(on_column_change_cb), NULL);

    gtk_tree_view_set_model(GTK_TREE_VIEW(s_reported_treeview), GTK_TREE_MODEL(s_reported_dumps_list_store));

    /* buttons are homogenous so set size only for one button and it will
     * work for the rest buttons in same gtk_hbox_new() */
    GtkWidget *btn_report = gtk_button_new_with_label(_("Report"));
    gtk_widget_set_size_request(btn_report, 200, 30);

    GtkWidget *btn_delete = gtk_button_new_from_stock(GTK_STOCK_DELETE);

    GtkWidget *hbox_report_delete = gtk_hbox_new(true, 0);
    gtk_box_pack_start(GTK_BOX(hbox_report_delete), btn_delete, true, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox_report_delete), btn_report, true, true, 10);

    GtkWidget *halign = gtk_alignment_new(1, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(halign), hbox_report_delete);

    gtk_box_pack_start(GTK_BOX(main_vbox), halign, false, false, 10);

    /* put the main_vbox to main window */
    gtk_container_add(GTK_CONTAINER(g_main_window), main_vbox);

    /* Double click/Enter handler */
    g_signal_connect(s_reported_treeview, "row-activated", G_CALLBACK(on_row_activated_cb), NULL);
    g_signal_connect(s_reported_treeview, "focus", G_CALLBACK(on_focus_cb), s_treeview);
    g_signal_connect(s_reported_treeview, "select-cursor-row", G_CALLBACK(on_select_cursor_row_cb), s_treeview);
    g_signal_connect(s_reported_treeview, "cursor-changed", G_CALLBACK(on_cursor_changed_cb), s_treeview);
    /* Delete handler */
    g_signal_connect(s_reported_treeview, "key-press-event", G_CALLBACK(on_key_press_event_cb), NULL);

    /* Open and delete button handlers */
    g_signal_connect(btn_report, "clicked", G_CALLBACK(on_btn_report_cb), NULL);
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_btn_delete_cb), NULL);

    /* Quit when user closes the main window */
    g_signal_connect(g_main_window, "destroy", gtk_main_quit, NULL);

    return g_main_window;
}


static int inotify_fd = -1;
static GIOChannel *channel_inotify;
static int channel_inotify_event_id = -1;
static char **s_dirs;
static int s_signal_pipe[2];


static void rescan_and_refresh(void)
{
    GtkTreePath *old_path = get_cursor();
    //log("old_path1:'%s'", gtk_tree_path_to_string(old_path)); //leak

    rescan_dirs_and_add_to_dirlist();

    /* Try to restore cursor position */
    //log("old_path2:'%s'", gtk_tree_path_to_string(old_path)); //leak
    sanitize_cursor(old_path);
    if (old_path)
        gtk_tree_path_free(old_path);
}


static void handle_signal(int signo)
{
    int save_errno = errno;

    // Enable for debugging only, malloc/printf are unsafe in signal handlers
    //VERB3 log("Got signal %d", signo);

    uint8_t sig_caught = signo;
    if (write(s_signal_pipe[1], &sig_caught, 1))
        /* we ignore result, if () shuts up stupid compiler */;

    errno = save_errno;
}

static gboolean handle_signal_pipe(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    /* It can only be SIGCHLD. Therefore we do not check the result,
     * and eat more than one byte at once: why refresh twice?
     */
    gchar buf[16];
    gsize bytes_read;
    g_io_channel_read_chars(gio, buf, sizeof(buf), &bytes_read, NULL);

    /* Destroy zombies */
    while (safe_waitpid(-1, NULL, WNOHANG) > 0)
        continue;

    rescan_and_refresh();

    return TRUE; /* "please don't remove this event" */
}


static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused);

static void init_notify(void)
{
    VERB1 log("Initializing inotify");
    errno = 0;
    inotify_fd = inotify_init();
    if (inotify_fd < 0)
        perror_msg("inotify_init failed");
    else
    {
        close_on_exec_on(inotify_fd);
        ndelay_on(inotify_fd);
        VERB1 log("Adding inotify watch to glib main loop");
        channel_inotify = g_io_channel_unix_new(inotify_fd);
        channel_inotify_event_id = g_io_add_watch(channel_inotify,
                                                  G_IO_IN,
                                                  handle_inotify_cb,
                                                  NULL);
    }
}

#if 0 // UNUSED
static void close_notify(void)
{
    if (inotify_fd >= 0)
    {
        //VERB1 log("g_source_remove:");
        g_source_remove(channel_inotify_event_id);
        //VERB1 log("g_io_channel_unref:");
        g_io_channel_unref(channel_inotify);
        //VERB1 log("close(inotify_fd):");
        close(inotify_fd);
        inotify_fd = -1;
        //VERB1 log("Done");
    }
}
#endif

/* Inotify handler */
static gboolean handle_inotify_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    /* Since dump dir creation usually involves directory rename as a last step,
     * we end up rescanning twice. A small wait after first inotify event
     * usually allows to avoid this.
     */
    usleep(10*1000);

    /* We read inotify events, but don't analyze them */
    gchar buf[sizeof(struct inotify_event) + PATH_MAX + 64];
    gsize bytes_read;
    while (g_io_channel_read_chars(gio, buf, sizeof(buf), &bytes_read, NULL) == G_IO_STATUS_NORMAL
        && bytes_read > 0
    ) {
        continue;
    }

    rescan_and_refresh();

    return TRUE; /* "please don't remove this event" */
}

static void scan_directory_and_add_to_dirlist(const char *path)
{
    GError *error = NULL;
    GList *problem_dirs = get_problems_over_dbus(path, &error);

    if (error)
    {
        char *message = xasprintf(_("Can't get problem list from abrt-dbus: %s"), error->message);
        show_warning_dialog(message, NULL);
        g_error_free(error);
        free(message);
        gtk_main_quit();
    }

    if (problem_dirs)
       g_list_foreach(problem_dirs, (GFunc)add_directory_to_dirlist, NULL);
    else
       /* we can't die here, NULL == empty list which means there might be no
        * problems in the given directory
        */
       VERB1 log("directory %s doesn't contain any problem directories", path);

    if (inotify_fd >= 0 && inotify_add_watch(inotify_fd, path, 0
    //      | IN_ATTRIB         // Metadata changed
    //      | IN_CLOSE_WRITE    // File opened for writing was closed
            | IN_CREATE         // File/directory created in watched directory
            | IN_DELETE         // File/directory deleted from watched directory
            | IN_DELETE_SELF    // Watched file/directory was itself deleted
            | IN_MODIFY         // File was modified
            | IN_MOVE_SELF      // Watched file/directory was itself moved
            | IN_MOVED_FROM     // File moved out of watched directory
            | IN_MOVED_TO       // File moved into watched directory
    ) < 0)
    {
        perror_msg("inotify_add_watch failed on '%s'", path);
    }
}

static void scan_dirs_and_add_to_dirlist(void)
{
    char **argv = s_dirs;
    while (*argv)
        scan_directory_and_add_to_dirlist(*argv++);
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    /* without this the name is set to argv[0] which confuses
     * desktops which use the name to find the corresponding .desktop file
     * trac#180
     */
    g_set_prgname("abrt");

    gtk_init(&argc, &argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-vp] [DIR]...\n"
        "\n"
        "Shows list of ABRT dump directories in specified DIR(s)\n"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_p = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(   'p', NULL, NULL      , _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    load_user_settings("abrt-gui");

    export_abrt_envvars(opts & OPT_p);

    GtkWidget *main_window = create_main_window();

    load_abrt_conf();
    const char *default_dirs[] = {
        g_settings_dump_location,
        NULL,
        NULL,
    };
    argv += optind;
    if (!argv[0])
    {
        char *home = getenv("HOME");
        if (home)
            default_dirs[1] = concat_path_file(home, ".abrt/spool");
        argv = (char**)default_dirs;
    }
    s_dirs = argv;

    init_notify();

    scan_dirs_and_add_to_dirlist();

    gtk_widget_show_all(main_window);

    sanitize_cursor(NULL);

    /* Set up signal pipe */
    xpipe(s_signal_pipe);
    close_on_exec_on(s_signal_pipe[0]);
    close_on_exec_on(s_signal_pipe[1]);
    ndelay_on(s_signal_pipe[0]);
    ndelay_on(s_signal_pipe[1]);
    signal(SIGCHLD, handle_signal);
    g_io_add_watch(g_io_channel_unix_new(s_signal_pipe[0]),
                G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                handle_signal_pipe,
                NULL);

    /* Enter main loop */
    gtk_main();

    save_user_settings();
    free_abrt_conf_data();
    return 0;
}
