#include <dbus/dbus.h>
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

#include "abrt_problems2_entry.h"
#include "abrt_problems2_service.h"


static GMainLoop *loop;
static guint g_timeout_source;
/* default, settable with -t: */
static unsigned g_timeout_value = 120;
static guint g_signal_crash;
static guint g_signal_dup_crash;

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

static void kill_timeout(void)
{
    if (g_timeout_source == 0)
        return;

    log_info("Removing timeout");
    guint tm = g_timeout_source;
    g_timeout_source = 0;
    g_source_remove(tm);
}

static void run_timeout(void)
{
    if (g_timeout_source != 0)
        return;

    log_info("Setting a new timeout");
    g_timeout_source = g_timeout_add_seconds(g_timeout_value, on_timeout_cb, NULL);
}


bool allowed_problem_dir(const char *dir_name)
{
    if (!dir_is_in_dump_location(dir_name))
    {
        error_msg("Bad problem directory name '%s', should start with: '%s'", dir_name, g_settings_dump_location);
        return false;
    }

    if (!dir_has_correct_permissions(dir_name, DD_PERM_DAEMONS))
    {
        error_msg("Problem directory '%s' has invalid owner, groop or mode", dir_name);
        return false;
    }

    return true;
}

bool allowed_problem_element(GDBusMethodInvocation *invocation, const char *element)
{
    if (str_is_correct_filename(element))
        return true;

    log_notice("'%s' is not a valid element name", element);
    char *error = xasprintf(_("'%s' is not a valid element name"), element);
    g_dbus_method_invocation_return_dbus_error(invocation,
            "org.freedesktop.problems.InvalidElement",
            error);

    free(error);
    return false;
}

static char *handle_new_problem(GVariant *problem_info, uid_t caller_uid, char **error)
{
    char *problem_id = NULL;
    problem_data_t *pd = problem_data_new();

    GVariantIter *iter;
    g_variant_get(problem_info, "a{ss}", &iter);
    gchar *key, *value;
    while (g_variant_iter_loop(iter, "{ss}", &key, &value))
    {
        if (allowed_new_user_problem_entry(caller_uid, key, value) == false)
        {
            *error = xasprintf("You are not allowed to create element '%s' containing '%s'", key, value);
            goto finito;
        }

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

    problem_id = problem_data_save(pd);
    if (problem_id)
        notify_new_path(problem_id);
    else if (error)
        *error = xasprintf("Cannot create a new problem");

finito:
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

enum {
    OPEN_FAIL_NO_REPLY = 1 << 0,
    OPEN_AUTH_ASK      = 1 << 1,
    OPEN_AUTH_FAIL     = 1 << 2,
};

static struct dump_dir *open_dump_directory(GDBusMethodInvocation *invocation,
    const gchar *caller, uid_t caller_uid, const char *problem_dir, int dd_flags, int flags)
{
    if (!allowed_problem_dir(problem_dir))
    {
        log_warning("UID=%d attempted to access not allowed problem directory '%s'",
                caller_uid, problem_dir);
        if (!(flags & OPEN_FAIL_NO_REPLY))
            return_InvalidProblemDir_error(invocation, problem_dir);
        return NULL;
    }

    struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_FD_ONLY);
    if (dd == NULL)
    {
        perror_msg("can't open problem directory '%s'", problem_dir);
        if (!(flags & OPEN_FAIL_NO_REPLY))
            return_InvalidProblemDir_error(invocation, problem_dir);
        return NULL;
    }

    if (!dd_accessible_by_uid(dd, caller_uid))
    {
        if (errno == ENOTDIR)
        {
            log_notice("Requested directory does not exist '%s'", problem_dir);
            if (!(flags & OPEN_FAIL_NO_REPLY))
                return_InvalidProblemDir_error(invocation, problem_dir);
            dd_close(dd);
            return NULL;
        }

        if (   !(flags & OPEN_AUTH_ASK)
            || polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            log_notice("not authorized");
            if (!(flags & OPEN_FAIL_NO_REPLY))
                g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            dd_close(dd);
            return NULL;
        }
    }

    dd = dd_fdopendir(dd, dd_flags);
    if (dd == NULL)
    {
        log_notice("Can't open the problem '%s' with flags %x0", problem_dir, dd_flags);
        if (!(flags & OPEN_FAIL_NO_REPLY))
            g_dbus_method_invocation_return_dbus_error(invocation,
                                "org.freedesktop.problems.Failure",
                                _("Can't open the problem"));
    }
    return dd;
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

    return open_dump_directory(invocation, /*caller*/NULL, caller_uid, problem_id, /*Read/Write*/0,
                               OPEN_AUTH_FAIL);
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
    uid_t caller_uid;
    GVariant *response;

    GError *error = NULL;
    caller_uid = abrt_p2_service_caller_uid(ABRT_P2_SERVICE(user_data), caller, &error);
    if (caller_uid == (uid_t) -1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    log_notice("caller_uid:%ld method:'%s'", (long)caller_uid, method_name);

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

        struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_FD_ONLY);
        if (dd == NULL)
        {
            perror_msg("can't open problem directory '%s'", problem_dir);
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int ddstat = dd_stat_for_uid(dd, caller_uid);
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

            dd_close(dd);
            return;
        }

        /* It might happen that we will do chowing even if the UID is alreay fs
         * owner, but DD_STAT_OWNED_BY_UID no longer denotes fs owner and this
         * method has to ensure file system ownership for the uid.
         */

        if ((ddstat & DD_STAT_ACCESSIBLE_BY_UID) == 0 &&
                polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            log_notice("not authorized");
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            dd_close(dd);
            return;
        }

        dd = dd_fdopendir(dd, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
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

        struct dump_dir *dd = open_dump_directory(invocation, caller, caller_uid,
                problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES , OPEN_AUTH_ASK);
        if (!dd)
            return;

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

        struct dump_dir *dd = open_dump_directory(invocation, caller, caller_uid,
                    problem_id, DD_OPEN_READONLY, OPEN_AUTH_ASK);
        if (!dd)
            return;

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
                                                    (gint32)element_info->flags,
                                                    (guint64)size,
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

        if (!allowed_problem_element(invocation, element))
            return;

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

        if (!allowed_problem_element(invocation, element))
            return;

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

        if (!allowed_problem_element(invocation, element))
            return;

        struct dump_dir *dd = open_dump_directory(invocation, caller, caller_uid,
                problem_id, DD_OPEN_READONLY, OPEN_AUTH_ASK);
        if (!dd)
            return;

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

            struct dump_dir *dd = open_dump_directory(invocation, caller, caller_uid,
                        dir_name, /*Read/Write*/0, OPEN_FAIL_NO_REPLY | OPEN_AUTH_ASK);

            if (dd)
            {
                if (dd_delete(dd) != 0)
                {
                    error_msg("Failed to delete problem directory '%s'", dir_name);
                    dd_close(dd);
                }
            }
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

        if (!allowed_problem_element(invocation, element))
            return;

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

static void handle_abrtd_problem_signals(GDBusConnection *connection,
            const gchar     *sender_name,
            const gchar     *object_path,
            const gchar     *interface_name,
            const gchar     *signal_name,
            GVariant        *parameters,
            gpointer         user_data)
{
    const char *dir;
    g_variant_get (parameters, "(&s)", &dir);

    log_debug("Caught '%s' signal from abrtd: '%s'", signal_name, dir);
    AbrtP2Service *service = ABRT_P2_SERVICE(user_data);

    GError *error = NULL;
    AbrtP2Object *obj = abrt_p2_service_get_entry_for_problem(service,
                                                              dir,
                                                              ABRT_P2_SERVICE_ENTRY_LOOKUP_OPTIONAL,
                                                              &error);
    if (error)
    {
        log_warning("Cannot notify '%s': failed to find entry: %s", dir, error->message);
        g_error_free(error);
        return;
    }

    if (obj == NULL)
    {
        AbrtP2Entry *entry = abrt_p2_entry_new_with_state(xstrdup(dir), ABRT_P2_ENTRY_STATE_COMPLETE);
        if (entry == NULL)
        {
            log_warning("Cannot notify '%s': failed to access data", dir);
            return;
        }

        obj = abrt_p2_service_register_entry(service, entry, &error);
        if (error)
        {
            log_warning("Cannot notify '%s': failed to register entry: %s", dir, error->message);
            g_error_free(error);
            return;
        }
    }

    AbrtP2Entry *entry = ABRT_P2_ENTRY(abrt_p2_object_get_node(obj));
    if (abrt_p2_entry_state(entry) != ABRT_P2_ENTRY_STATE_COMPLETE)
    {
        log_debug("Not notifying temporary/deleted problem directory: %s", dir);
        return;
    }

    abrt_p2_service_notify_entry_object(service, obj, &error);
    if (error)
    {
        log_warning("Failed to notify '%s': %s", dir, error->message);
        g_error_free(error);
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
                                                       user_data,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
    g_assert(registration_id > 0);

    GError *error = NULL;

    int r = abrt_p2_service_register_objects(ABRT_P2_SERVICE(user_data), connection, &error);
    if (r == 0 || r == -EALREADY)
    {
        g_signal_crash = g_dbus_connection_signal_subscribe(connection,
                                                            NULL,
                                                            "org.freedesktop.Problems2",
                                                            "ImportProblem",
                                                            "/org/freedesktop/Problems2",
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                                            handle_abrtd_problem_signals,
                                                            user_data, NULL);

        g_signal_dup_crash = g_dbus_connection_signal_subscribe(connection,
                                                            NULL,
                                                            "org.freedesktop.Problems2",
                                                            "ReloadProblem",
                                                            "/org/freedesktop/Problems2",
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                                            handle_abrtd_problem_signals,
                                                            user_data, NULL);

        run_timeout();
        return;
    }

    error_msg("Failed to register Problems2 Objects: %s", error->message);
    g_error_free(error);

    g_main_loop_quit(loop);
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

void configure_problems2_service(AbrtP2Service *p2_service)
{
    struct env_option {
        const char *name;
        void (*setter_unsigned)(AbrtP2Service *s, uid_t u, unsigned v);
        void (*setter_off_t)(AbrtP2Service *s, uid_t u, off_t v);
    } env_options[] = {
        {   .name = "ABRT_DBUS_USER_CLIENTS",
            .setter_unsigned = abrt_p2_service_set_user_clients_limit,
        },
        {   .name = "ABRT_DBUS_ELEMENTS_LIMIT",
             .setter_unsigned = abrt_p2_service_set_elements_limit,
        },
        {   .name = "ABRT_DBUS_PROBLEMS_LIMIT",
            .setter_unsigned = abrt_p2_service_set_user_problems_limit,
        },
        {   .name = "ABRT_DBUS_NEW_PROBLEM_THROTTLING_MAGNITUDE",
            .setter_unsigned = abrt_p2_service_set_new_problem_throttling_magnitude,
        },
        {   .name = "ABRT_DBUS_NEW_PROBLEMS_BATCH",
            .setter_unsigned = abrt_p2_service_set_new_problems_batch,
        },
        {   .name = "ABRT_DBUS_DATA_SIZE_LIMIT",
            .setter_off_t = abrt_p2_service_set_data_size_limit,
        },
    };

    for (size_t i = 0; i < sizeof(env_options)/sizeof(env_options[0]); ++i)
    {
        const char *value = getenv(env_options[i].name);
        if (value == NULL)
            continue;

        errno = 0;
        char *end = NULL;
        const unsigned long limit = strtoul(value, &end, 10);
        if (errno || value == end || *end != '\0')
            error_msg_and_die("not a number in environment '%s': %s", env_options[i].name, value);

        if (env_options[i].setter_unsigned)
        {
            if (limit > UINT_MAX)
                error_msg_and_die("an out of range number in environment '%s': %s", env_options[i].name, value);

            env_options[i].setter_unsigned(p2_service, (uid_t)-1, (unsigned int)limit);
        }
        else if (env_options[i].setter_off_t)
        {
            const off_t off_t_limit = limit;
            env_options[i].setter_off_t(p2_service, (uid_t)-1, off_t_limit);
        }
        else
            error_msg_and_die("Bug: invalid parser of environment values");

        log_debug("Used environment variable: %s", env_options[i].name);
    }
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

    msg_prefix = "abrt-dbus"; /* for log_warning(), error_msg() and such */

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

    AbrtP2Service *p2_service = abrt_p2_service_new(&err);
    if (p2_service == NULL)
        error_msg_and_die("Failed to initialize Problems2 service: %s", err->message);

    g_signal_connect(p2_service, "new-client-connected", G_CALLBACK(kill_timeout), NULL);
    g_signal_connect(p2_service, "all-clients-disconnected", G_CALLBACK(run_timeout), NULL);

    DBusConnection *con = dbus_connection_open("org.freedesktop.DBus", NULL);

    /* FIXME: I'm sorry but I'm not able to find out why the maximum message
     * length limit is around 200kiB but the official configuration says
     * something about 128MiB. Is it a bug in this code? */
    /*long max_message_size = DBUS_MAXIMUM_MESSAGE_LENGTH;*/

    long max_message_unix_fds = 16;
    if (con != NULL)
    {
        /*max_message_size = dbus_connection_get_max_message_size(con);*/
        max_message_unix_fds = dbus_connection_get_max_message_unix_fds(con);
        dbus_connection_close(con);
    }
    /*abrt_p2_service_set_max_message_size(p2_service, max_message_size);*/
    abrt_p2_service_set_max_message_size(p2_service, 200000L);
    abrt_p2_service_set_max_message_unix_fds(p2_service, max_message_unix_fds);

    configure_problems2_service(p2_service);

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                             ABRT_DBUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             NULL,
                             on_name_lost,
                             p2_service,
                             g_object_unref);

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
