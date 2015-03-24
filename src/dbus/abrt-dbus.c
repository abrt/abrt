#include <gio/gio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "libabrt.h"
#include "abrt-polkit.h"
#include "abrt_glib.h"
#include <libreport/dump_dir.h>
#include "problem_api.h"

static GMainLoop *loop;
static guint g_timeout_source;
/* default, settable with -t: */
static unsigned g_timeout_value = 120;

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='"ABRT_DBUS_IFACE"'>"
  "    <method name='NewProblem'>"
  "      <arg type='a{ss}' name='problem_data' direction='in'/>"
  "      <arg type='s' name='problem_id' direction='out'/>"
  "    </method>"
  "    <method name='GetProblems'>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetAllProblems'>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetForeignProblems'>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetInfo'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='as' name='element_names' direction='in'/>"
  "      <arg type='a{ss}' name='response' direction='out'/>"
  "    </method>"
  "    <method name='SetElement'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='s' name='name' direction='in'/>"
  "      <arg type='s' name='value' direction='in'/>"
  "    </method>"
  "    <method name='DeleteElement'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='s' name='name' direction='in'/>"
  "    </method>"
  "    <method name='TestElementExists'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='s' name='name' direction='in'/>"
  "      <arg type='b' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetProblemData'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='a{s(its)}' name='problem_data' direction='out'/>"
  "    </method>"
  "    <method name='ChownProblemDir'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "    </method>"
  "    <method name='DeleteProblem'>"
  "      <arg type='as' name='problem_dir' direction='in'/>"
  "    </method>"
  "    <method name='FindProblemByElementInTimeRange'>"
  "      <arg type='s' name='element' direction='in'/>"
  "      <arg type='s' name='value' direction='in'/>"
  "      <arg type='x' name='timestamp_from' direction='in'/>"
  "      <arg type='x' name='timestamp_to' direction='in'/>"
  "      <arg type='b' name='all_users' direction='in'/>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='Quit' />"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

/* forward */
static gboolean on_timeout_cb(gpointer user_data);

static void reset_timeout(void)
{
    if (g_timeout_source > 0)
    {
        log_info("Removing timeout");
        g_source_remove(g_timeout_source);
    }
    log_info("Setting a new timeout");
    g_timeout_source = g_timeout_add_seconds(g_timeout_value, on_timeout_cb, NULL);
}

static uid_t get_caller_uid(GDBusConnection *connection, GDBusMethodInvocation *invocation, const char *caller)
{
    GError *error = NULL;
    guint caller_uid;

    GDBusProxy * proxy = g_dbus_proxy_new_sync(connection,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     NULL,
                                     "org.freedesktop.DBus",
                                     "/org/freedesktop/DBus",
                                     "org.freedesktop.DBus",
                                     NULL,
                                     &error);

    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                     "GetConnectionUnixUser",
                                     g_variant_new ("(s)", caller),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);

    if (result == NULL)
    {
        /* we failed to get the uid, so return (uid_t) -1 to indicate the error
         */
        if (error)
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidUser",
                                      error->message);
            g_error_free(error);
        }
        else
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidUser",
                                      _("Unknown error"));
        }
        return (uid_t) -1;
    }

    g_variant_get(result, "(u)", &caller_uid);
    g_variant_unref(result);

    log_info("Caller uid: %i", caller_uid);
    return caller_uid;
}

static bool allowed_problem_dir(const char *dir_name)
{
//HACK HACK HACK! Disabled for now until we fix clients (abrt-gui) to not pass /home/user/.cache/abrt/spool
#if 0
    unsigned len = strlen(g_settings_dump_location);

    /* If doesn't start with "g_settings_dump_location[/]"... */
    if (strncmp(dir_name, g_settings_dump_location, len) != 0
     || (dir_name[len] != '/' && dir_name[len] != '\0')
    /* or contains "/." anywhere (-> might contain ".." component) */
     || strstr(dir_name + len, "/.")
    ) {
        return false;
    }
#endif
    return true;
}

static char *handle_new_problem(GVariant *problem_info, uid_t caller_uid, char **error)
{
    problem_data_t *pd = problem_data_new();

    GVariantIter *iter;
    g_variant_get(problem_info, "a{ss}", &iter);
    gchar *key, *value;
    while (g_variant_iter_loop(iter, "{ss}", &key, &value))
    {
        problem_data_add_text_editable(pd, key, value);
    }

    if (caller_uid != 0 || problem_data_get_content_or_NULL(pd, FILENAME_UID) == NULL)
    {   /* set uid field to caller's uid if caller is not root or root doesn't pass own uid */
        log_info("Adding UID %d to problem data", caller_uid);
        char buf[sizeof(uid_t) * 3 + 2];
        snprintf(buf, sizeof(buf), "%d", caller_uid);
        problem_data_add_text_noteditable(pd, FILENAME_UID, buf);
    }

    /* At least it should generate local problem identifier UUID */
    problem_data_add_basics(pd);

    char *problem_id = problem_data_save(pd);
    if (problem_id)
        notify_new_path(problem_id);
    else if (error)
        *error = xasprintf("Cannot create a new problem");

    problem_data_free(pd);
    return problem_id;
}

static void return_InvalidProblemDir_error(GDBusMethodInvocation *invocation, const char *dir_name)
{
    char *msg = xasprintf(_("'%s' is not a valid problem directory"), dir_name);
    g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidProblemDir",
                                      msg);

    free(msg);
}

/*
 * Checks element's rights and does not open directory if element is protected.
 * Checks problem's rights and does not open directory if user hasn't got
 * access to a problem.
 *
 * Returns a dump directory opend for writing or NULL.
 *
 * If any operation from the above listed fails, immediately returns D-Bus
 * error to a D-Bus caller.
 */
static struct dump_dir *open_directory_for_modification_of_element(
    GDBusMethodInvocation *invocation,
    uid_t caller_uid,
    const char *problem_id,
    const char *element)
{
    static const char *const protected_elements[] = {
        FILENAME_TIME,
        FILENAME_UID,
        NULL,
    };

    for (const char *const *protected = protected_elements; *protected; ++protected)
    {
        if (strcmp(*protected, element) == 0)
        {
            log_notice("'%s' element of '%s' can't be modified", element, problem_id);
            char *error = xasprintf(_("'%s' element can't be modified"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                        "org.freedesktop.problems.ProtectedElement",
                                        error);
            free(error);
            return NULL;
        }
    }

    if (!dump_dir_accessible_by_uid(problem_id, caller_uid))
    {
        if (errno == ENOTDIR)
        {
            log_notice("'%s' is not a valid problem directory", problem_id);
            return_InvalidProblemDir_error(invocation, problem_id);
        }
        else
        {
            log_notice("UID(%d) is not Authorized to access '%s'", caller_uid, problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                "org.freedesktop.problems.AuthFailure",
                                _("Not Authorized"));
        }

        return NULL;
    }

    struct dump_dir *dd = dd_opendir(problem_id, /* flags : */ 0);
    if (!dd)
    {   /* This should not happen because of the access check above */
        log_notice("Can't access the problem '%s' for modification", problem_id);
        g_dbus_method_invocation_return_dbus_error(invocation,
                                "org.freedesktop.problems.Failure",
                                _("Can't access the problem for modification"));
        return NULL;
    }

    return dd;
}


/*
 * Lists problems which have given element and were seen in given time interval
 */

struct field_and_time_range {
    GList *list;
    const char *element;
    const char *value;
    unsigned long timestamp_from;
    unsigned long timestamp_to;
};

static int add_dirname_to_GList_if_matches(struct dump_dir *dd, void *arg)
{
    struct field_and_time_range *me = arg;

    char *field_data = dd_load_text(dd, me->element);
    int brk = (strcmp(field_data, me->value) != 0);
    free(field_data);
    if (brk)
        return 0;

    field_data = dd_load_text(dd, FILENAME_LAST_OCCURRENCE);
    long val = atol(field_data);
    free(field_data);
    if (val < me->timestamp_from || val > me->timestamp_to)
        return 0;

    me->list = g_list_prepend(me->list, xstrdup(dd->dd_dirname));
    return 0;
}

static GList *get_problem_dirs_for_element_in_time(uid_t uid,
                const char *element,
                const char *value,
                unsigned long timestamp_from,
                unsigned long timestamp_to)
{
    if (timestamp_to == 0) /* not sure this is possible, but... */
        timestamp_to = time(NULL);

    struct field_and_time_range me = {
        .list = NULL,
        .element = element,
        .value = value,
        .timestamp_from = timestamp_from,
        .timestamp_to = timestamp_to,
    };

    for_each_problem_in_dir(g_settings_dump_location, uid, add_dirname_to_GList_if_matches, &me);

    return g_list_reverse(me.list);
}


static void handle_method_call(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant    *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer    user_data)
{
    reset_timeout();

    uid_t caller_uid;
    GVariant *response;

    caller_uid = get_caller_uid(connection, invocation, caller);

    log_notice("caller_uid:%ld method:'%s'", (long)caller_uid, method_name);

    if (caller_uid == (uid_t) -1)
        return;

    if (g_strcmp0(method_name, "NewProblem") == 0)
    {
        char *error = NULL;
        char *problem_id = handle_new_problem(g_variant_get_child_value(parameters, 0), caller_uid, &error);
        if (!problem_id)
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      error);
            free(error);
            return;
        }
        /* else */
        response = g_variant_new("(s)", problem_id);
        g_dbus_method_invocation_return_value(invocation, response);
        free(problem_id);

        return;
    }

    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        GList *dirs = get_problem_dirs_for_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        //I was told that g_dbus_method frees the response
        //g_variant_unref(response);
        return;
    }

    if (g_strcmp0(method_name, "GetAllProblems") == 0)
    {
        /*
        - so, we have UID,
        - if it's 0, then we don't have to check anything and just return all directories
        - if uid != 0 then we want to ask for authorization
        */
        if (caller_uid != 0)
        {
            if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") == PolkitYes)
                caller_uid = 0;
        }

        GList * dirs = get_problem_dirs_for_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);

        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "GetForeignProblems") == 0)
    {
        GList * dirs = get_problem_dirs_not_accessible_by_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "ChownProblemDir") == 0)
    {
        const gchar *problem_dir;
        g_variant_get(parameters, "(&s)", &problem_dir);
        log_notice("problem_dir:'%s'", problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int ddstat = dump_dir_stat_for_uid(problem_dir, caller_uid);
        if (ddstat < 0)
        {
            if (errno == ENOTDIR)
            {
                log_notice("requested directory does not exist '%s'", problem_dir);
            }
            else
            {
                perror_msg("can't get stat of '%s'", problem_dir);
            }

            return_InvalidProblemDir_error(invocation, problem_dir);

            return;
        }

        if (ddstat & DD_STAT_OWNED_BY_UID)
        {   //caller seems to be in group with access to this dir, so no action needed
            log_notice("caller has access to the requested directory %s", problem_dir);
            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }

        if ((ddstat & DD_STAT_ACCESSIBLE_BY_UID) == 0 &&
                polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            log_notice("not authorized");
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            return;
        }

        struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int chown_res = dd_chown(dd, caller_uid);
        if (chown_res != 0)
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.ChownError",
                                              _("Chowning directory failed. Check system logs for more details."));
        else
            g_dbus_method_invocation_return_value(invocation, NULL);

        dd_close(dd);
        return;
    }

    if (g_strcmp0(method_name, "GetInfo") == 0)
    {
        /* Parameter tuple is (sas) */

	/* Get 1st param - problem dir name */
        const gchar *problem_dir;
        g_variant_get_child(parameters, 0, "&s", &problem_dir);
        log_notice("problem_dir:'%s'", problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        if (!dump_dir_accessible_by_uid(problem_dir, caller_uid))
        {
            if (errno == ENOTDIR)
            {
                log_notice("Requested directory does not exist '%s'", problem_dir);
                return_InvalidProblemDir_error(invocation, problem_dir);
                return;
            }

            if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
            {
                log_notice("not authorized");
                g_dbus_method_invocation_return_dbus_error(invocation,
                                                  "org.freedesktop.problems.AuthFailure",
                                                  _("Not Authorized"));
                return;
            }
        }

        struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

	/* Get 2nd param - vector of element names */
        GVariant *array = g_variant_get_child_value(parameters, 1);
        GList *elements = string_list_from_variant(array);
        g_variant_unref(array);

        GVariantBuilder *builder = NULL;
        for (GList *l = elements; l; l = l->next)
        {
            const char *element_name = (const char*)l->data;
            char *value = dd_load_text_ext(dd, element_name, 0
                                                | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                                                | DD_FAIL_QUIETLY_ENOENT
                                                | DD_FAIL_QUIETLY_EACCES);
            log_notice("element '%s' %s", element_name, value ? "fetched" : "not found");
            if (value)
            {
                if (!builder)
                    builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

                /* g_variant_builder_add makes a copy. No need to xstrdup here */
                g_variant_builder_add(builder, "{ss}", element_name, value);
                free(value);
            }
        }
        list_free_with_free(elements);
        dd_close(dd);
        /* It is OK to call g_variant_new("(a{ss})", NULL) because */
        /* G_VARIANT_TYPE_TUPLE allows NULL value */
        GVariant *response = g_variant_new("(a{ss})", builder);

        if (builder)
            g_variant_builder_unref(builder);

        log_info("GetInfo: returning value for '%s'", problem_dir);
        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "GetProblemData") == 0)
    {
        /* Parameter tuple is (s) */
        const char *problem_id;

        g_variant_get(parameters, "(&s)", &problem_id);

        int ddstat = dump_dir_stat_for_uid(problem_id, caller_uid);
        if ((ddstat & DD_STAT_ACCESSIBLE_BY_UID) == 0 &&
                polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            log_notice("Unauthorized access : '%s'", problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            return;
        }

        struct dump_dir *dd = dd_opendir(problem_id, DD_OPEN_READONLY);
        if (dd == NULL)
        {
            log_notice("Can't access the problem '%s' for reading", problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                    "org.freedesktop.problems.Failure",
                                    _("Can't access the problem for reading"));
            return;
        }

        problem_data_t *pd = create_problem_data_from_dump_dir(dd);
        dd_close(dd);

        GVariantBuilder *response_builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

        GHashTableIter pd_iter;
        char *element_name;
        struct problem_item *element_info;
        g_hash_table_iter_init(&pd_iter, pd);
        while (g_hash_table_iter_next(&pd_iter, (void**)&element_name, (void**)&element_info))
        {
            unsigned long size = 0;
            if (problem_item_get_size(element_info, &size) != 0)
            {
                log_notice("Can't get stat of : '%s'", element_info->content);
                continue;
            }

            g_variant_builder_add(response_builder, "{s(its)}",
                                                    element_name,
                                                    element_info->flags,
                                                    size,
                                                    element_info->content);
        }

        GVariant *response = g_variant_new("(a{s(its)})", response_builder);
        g_variant_builder_unref(response_builder);

        problem_data_free(pd);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "SetElement") == 0)
    {
        const char *problem_id;
        const char *element;
        const char *value;

        g_variant_get(parameters, "(&s&s&s)", &problem_id, &element, &value);

        if (element == NULL || element[0] == '\0' || strlen(element) > 64)
        {
            log_notice("'%s' is not a valid element name of '%s'", element, problem_id);
            char *error = xasprintf(_("'%s' is not a valid element name"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.InvalidElement",
                                              error);

            free(error);
            return;
        }

        struct dump_dir *dd = open_directory_for_modification_of_element(
                                    invocation, caller_uid, problem_id, element);
        if (!dd)
            /* Already logged from open_directory_for_modification_of_element() */
            return;

        /* Is it good idea to make it static? Is it possible to change the max size while a single run? */
        const double max_dir_size = g_settings_nMaxCrashReportsSize * (1024 * 1024);
        const long item_size = dd_get_item_size(dd, element);
        if (item_size < 0)
        {
            log_notice("Can't get size of '%s/%s'", problem_id, element);
            char *error = xasprintf(_("Can't get size of '%s'"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      error);
            return;
        }

        const double requested_size = (double)strlen(value) - item_size;
        /* Don't want to check the size limit in case of reducing of size */
        if (requested_size > 0
            && requested_size > (max_dir_size - get_dirsize(g_settings_dump_location)))
        {
            log_notice("No problem space left in '%s' (requested Bytes %f)", problem_id, requested_size);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      _("No problem space left"));
        }
        else
        {
            dd_save_text(dd, element, value);
            g_dbus_method_invocation_return_value(invocation, NULL);
        }

        dd_close(dd);

        return;
    }

    if (g_strcmp0(method_name, "DeleteElement") == 0)
    {
        const char *problem_id;
        const char *element;

        g_variant_get(parameters, "(&s&s)", &problem_id, &element);

        struct dump_dir *dd = open_directory_for_modification_of_element(
                                    invocation, caller_uid, problem_id, element);
        if (!dd)
            /* Already logged from open_directory_for_modification_of_element() */
            return;

        const int res = dd_delete_item(dd, element);
        dd_close(dd);

        if (res != 0)
        {
            log_notice("Can't delete the element '%s' from the problem directory '%s'", element, problem_id);
            char *error = xasprintf(_("Can't delete the element '%s' from the problem directory '%s'"), element, problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                          "org.freedesktop.problems.Failure",
                                          error);
            free(error);
            return;
        }


        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "TestElementExists") == 0)
    {
        const char *problem_id;
        const char *element;

        g_variant_get(parameters, "(&s&s)", &problem_id, &element);


        struct dump_dir *dd = dd_opendir(problem_id, DD_OPEN_READONLY);
        if (!dd)
        {
            log_notice("Can't access the problem '%s'", problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                    "org.freedesktop.problems.Failure",
                                    _("Can't access the problem"));
            return;
        }

        int ddstat = dump_dir_stat_for_uid(problem_id, caller_uid);
        if ((ddstat & DD_STAT_ACCESSIBLE_BY_UID) == 0 &&
                polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            dd_close(dd);
            log_notice("Unauthorized access : '%s'", problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            return;
        }

        int ret = dd_exist(dd, element);
        dd_close(dd);

        GVariant *response = g_variant_new("(b)", ret);
        g_dbus_method_invocation_return_value(invocation, response);

        return;
    }

    if (g_strcmp0(method_name, "DeleteProblem") == 0)
    {
        /* Dbus parameters are always tuples.
         * In this case, it's (as) - a tuple of one element (array of strings).
         * Need to fetch the array:
         */
        GVariant *array = g_variant_get_child_value(parameters, 0);
        GList *problem_dirs = string_list_from_variant(array);
        g_variant_unref(array);

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;
            log_notice("dir_name:'%s'", dir_name);
            if (!allowed_problem_dir(dir_name))
            {
                return_InvalidProblemDir_error(invocation, dir_name);
                goto ret;
            }
        }

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;
            if (!dump_dir_accessible_by_uid(dir_name, caller_uid))
            {
                if (errno == ENOTDIR)
                {
                    log_notice("Requested directory does not exist '%s'", dir_name);
                    continue;
                }

                if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
                { // if user didn't provide correct credentials, just move to the next dir
                    continue;
                }
            }
            delete_dump_dir(dir_name);
        }

        g_dbus_method_invocation_return_value(invocation, NULL);
 ret:
        list_free_with_free(problem_dirs);
        return;
    }

    if (g_strcmp0(method_name, "FindProblemByElementInTimeRange") == 0)
    {
        const gchar *element;
        const gchar *value;
        glong timestamp_from;
        glong timestamp_to;
        gboolean all;

        g_variant_get_child(parameters, 0, "&s", &element);
        g_variant_get_child(parameters, 1, "&s", &value);
        g_variant_get_child(parameters, 2, "x", &timestamp_from);
        g_variant_get_child(parameters, 3, "x", &timestamp_to);
        g_variant_get_child(parameters, 4, "b", &all);

        if (all && polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") == PolkitYes)
            caller_uid = 0;

        GList *dirs = get_problem_dirs_for_element_in_time(caller_uid, element, value, timestamp_from,
                                                        timestamp_to);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "Quit") == 0)
    {
        g_dbus_method_invocation_return_value(invocation, NULL);
        g_main_loop_quit(loop);
        return;
    }
}

static gboolean on_timeout_cb(gpointer user_data)
{
    g_main_loop_quit(loop);
    return TRUE;
}

static const GDBusInterfaceVTable interface_vtable =
{
    .method_call = handle_method_call,
    .get_property = NULL,
    .set_property = NULL,
};

static void on_bus_acquired(GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    guint registration_id;

    registration_id = g_dbus_connection_register_object(connection,
                                                       ABRT_DBUS_OBJECT,
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
    g_assert(registration_id > 0);

    reset_timeout();
}

/* not used
static void on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}
*/

static void on_name_lost(GDBusConnection *connection,
                      const gchar *name,
                      gpointer user_data)
{
    g_print(_("The name '%s' has been lost, please check if other "
              "service owning the name is not running.\n"), name);
    exit(1);
}

int main(int argc, char *argv[])
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
    guint owner_id;

    abrt_init(argv);

    const char *program_usage_string = _(
        "& [options]"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_t = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_INTEGER('t', NULL, &g_timeout_value, _("Exit after NUM seconds of inactivity")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    /* When dbus daemon starts us, it doesn't set PATH
     * (I saw it set only DBUS_STARTER_ADDRESS and DBUS_STARTER_BUS_TYPE).
     * In this case, set something sane:
     */
    const char *env_path = getenv("PATH");
    if (!env_path || !env_path[0])
        putenv((char*)"PATH=/usr/sbin:/usr/bin:/sbin:/bin");

    msg_prefix = "abrt-dbus"; /* for log(), error_msg() and such */

    if (getuid() != 0)
        error_msg_and_die(_("This program must be run as root."));

    glib_init();

    /* We are lazy here - we don't want to manually provide
    * the introspection data structures - so we just build
    * them from XML.
    */
    GError *err = NULL;
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &err);
    if (err != NULL)
        error_msg_and_die("Invalid D-Bus interface: %s", err->message);

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                             ABRT_DBUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             NULL,
                             on_name_lost,
                             NULL,
                             NULL);

    /* initialize the g_settings_dump_location */
    load_abrt_conf();

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    log_notice("Cleaning up");

    g_bus_unown_name(owner_id);

    g_dbus_node_info_unref(introspection_data);

    free_abrt_conf_data();

    return 0;
}
