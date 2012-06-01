#include <gio/gio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "libabrt.h"
#include "abrt-polkit.h"
#include "abrt-dbus.h"
#include <libreport/dump_dir.h>

GMainLoop *loop;
guint g_timeout;
static unsigned s_timeout;

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='"ABRT_DBUS_IFACE"'>"
  "    <method name='GetProblems'>"
  "      <arg type='s' name='directory' direction='in'/>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetAllProblems'>"
  "      <arg type='s' name='directory' direction='in'/>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetInfo'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='a{ss}' name='response' direction='out'/>"
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

static void reset_timeout()
{
    if (g_timeout > 0)
    {
        VERB2 log("Removing timeout");
        g_source_remove(g_timeout);
    }
    VERB2 log("Setting a new timeout");
    g_timeout = g_timeout_add_seconds(s_timeout, on_timeout_cb, NULL);
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

    VERB2 log("Caller uid: %i", caller_uid);
    return caller_uid;
}

static bool uid_in_group(uid_t uid, gid_t gid)
{
    char **tmp;
    struct passwd *pwd = getpwuid(uid);

    if (!pwd)
        return FALSE;

    if (pwd->pw_gid == gid)
        return TRUE;

    struct group *grp = getgrgid(gid);
    if (!(grp && grp->gr_mem))
        return FALSE;

    for (tmp = grp->gr_mem; *tmp != NULL; tmp++)
    {
        if (g_strcmp0(*tmp, pwd->pw_name) == 0)
        {
            VERB3 log("user %s belongs to group: %s",  pwd->pw_name, grp->gr_name);
            return TRUE;
        }
    }

    VERB2 log("user %s DOESN'T belong to group: %s",  pwd->pw_name, grp->gr_name);
    return FALSE;
}

/*
 0 - user doesn't have access
 1 - user has access
*/
static int dir_accessible_by_uid(const char *dir_path, uid_t uid)
{
    struct stat statbuf;
    if (stat(dir_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
    {
        if (uid == 0 || (statbuf.st_mode & S_IROTH) || uid_in_group(uid, statbuf.st_gid))
        {
            VERB1 log("caller has access to the requested directory %s", dir_path);
            return 1;
        }
    }

    return 0;
}

/*
 * Structure for simple conditions based on problem fields
 */
struct problem_condition
{
    /* a name of filed required by evaluate function */
    const char *field_name;
    /* extra data passed to evaluate function */
    const void *args;
    /* evaluate function returning TRUE if condition was passed */
    bool (*evaluate)(const char *, const void *);
};

/*
 * Evaluates a NULL-terminated list of problem conditions as a logical conjunction
 */
static bool problem_condition_evaluate_and(struct dump_dir *dd,
                                           const struct problem_condition *const *condition)
{
    /* We stop on the first FALSE condition */
    while (condition && *condition != NULL)
    {
        const struct problem_condition *c = *condition;
        char *field_data = dd_load_text(dd, c->field_name);
        bool value = c->evaluate(field_data, c->args);
        free(field_data);
        if (!value)
            return false;
        ++condition;
    }

    return true;
}

/*
 * Goes through all problems and selects only problems accessible by caller_uid and
 * problems for which an and_filter gets TRUE
 *
 * @param condition a NULL-terminated list of problem conditions evaluated
 * as conjunction, can be NULL (means always TRUE)
 */
static GList* scan_directory(const char *path,
                             uid_t caller_uid,
                             const struct problem_condition *const *condition)
{
    GList *list = NULL;

    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $XDG_CACHE_DIR/abrt/spool doesn't exist */
        //perror_msg("Can't open directory '%s'", path);
        return list;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */

        char *full_name = concat_path_file(path, dent->d_name);
        struct stat statbuf;
        if (stat(full_name, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
        {
            /* Silently ignore *any* errors, not only EACCES.
             * We saw "lock file is locked by process PID" error
             * when we raced with wizard.
             */
            int sv_logmode = logmode;
            logmode = 0;
            struct dump_dir *dd = dd_opendir(full_name, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
            logmode = sv_logmode;
            /* or we could just setuid?
             - but it would require locking, because we want to setuid back before we server another request..
            */
            if (dd && (caller_uid == 0 || statbuf.st_mode & S_IROTH || uid_in_group(caller_uid, statbuf.st_gid)))
            {
                if (problem_condition_evaluate_and(dd, condition))
                {
                    list = g_list_prepend(list, full_name);
                    full_name = NULL;
                }
            }
            dd_close(dd); //doesn't fail even if dd == NULL
        }
        free(full_name);
    }
    closedir(dp);

    /* Why reverse?
     * Because N*prepend+reverse is faster than N*append
     */
    return g_list_reverse(list);
}

/* Self explaining time interval structure */
struct time_interval
{
    unsigned long from;
    unsigned long to;
};

/*
 * A problem condition evaluate function for checking of the TIME field against
 * an allowed interval
 *
 * @param field_data a content from the PID field
 * @param args a pointer to an instance of struct time_interval
 * @return TRUE if a field value is in a specified interval; otherwise FALSE
 */
static bool time_interval_problem_condition(const char *field_data, const void *args)
{
    const struct time_interval *const interval = (const struct time_interval *)args;
    const time_t timestamp = atol(field_data);

    return interval->from <= timestamp && timestamp <= interval->to;
}

/*
 * A problem condition evaluate function passed if strings are equal
 *
 * @param field_data a content of a field
 * @param args a checked string
 * @return TRUE if both strings are equal; otherwise FALSE
 */
static bool equal_string_problem_condition(const char *field_data, const void *args)
{
    return !strcmp(field_data, (const char *)args);
}

static GVariant *get_problem_dirs_for_uid(uid_t uid, const char *dump_location)
{
    GList *dirs = scan_directory(dump_location, uid, NULL);

    return variant_from_string_list(dirs);
}

/*
 * Finds problems with the specified element and which were created in the interval
 */
static GVariant *get_problem_dirs_for_element_in_time(uid_t uid,
                                                      const char *element,
                                                      const char *value,
                                                      unsigned long timestamp_from,
                                                      unsigned long timestamp_to,
                                                      const char *dump_location)
{
    const struct problem_condition elementc = {
        .field_name = element,
        .args = value,
        .evaluate = equal_string_problem_condition,
    };

    const struct time_interval interval = {
        .from = timestamp_from,
        .to = timestamp_to
    };

    const struct problem_condition timec = {
        .field_name = FILENAME_TIME,
        .args = &interval,
        .evaluate = time_interval_problem_condition
    };

    const struct problem_condition *const condition[] = {
        &elementc,
        &timec,
        NULL
    };

    GList *dirs = scan_directory(dump_location, uid, condition);

    return variant_from_string_list(dirs);
}

static bool allowed_problem_dir(const char *dir_name)
{
    unsigned len = strlen(g_settings_dump_location);

    /* If doesn't start with "g_settings_dump_location[/]"... */
    if (strncmp(dir_name, g_settings_dump_location, len) != 0
     || (dir_name[len] != '/' && dir_name[len] != '\0')
    /* or contains "/." anywhere (-> might contain ".." component) */
     || strstr(dir_name + len, "/.")
    ) {
        return false;
    }
    return true;
}

static void return_InvalidProblemDir_error(GDBusMethodInvocation *invocation, const char *dir_name)
{
    char *msg = xasprintf(_("'%s' is not a valid problem directory"), dir_name);
    g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidProblemDir",
                                      msg);
    free(msg);
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

    VERB1 log("caller_uid:%ld method:'%s'", (long)caller_uid, method_name);

    if (caller_uid == (uid_t) -1)
        return;

    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        //TODO: change the API to not accept the dumpdir from user, but read it from config file?

        const gchar *dump_location;
        g_variant_get(parameters, "(&s)", &dump_location);

        if (!allowed_problem_dir(dump_location))
        {
            return_InvalidProblemDir_error(invocation, dump_location);
            return;
        }

        response = get_problem_dirs_for_uid(caller_uid, dump_location);

        g_dbus_method_invocation_return_value(invocation, response);
        //I was told that g_dbus_method frees the response
        //g_variant_unref(response);
        return;
    }

    if (g_strcmp0(method_name, "GetAllProblems") == 0)
    {
        const gchar *dump_location;
        g_variant_get(parameters, "(&s)", &dump_location);

        if (!allowed_problem_dir(dump_location))
        {
            return_InvalidProblemDir_error(invocation, dump_location);
            return;
        }

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

        response = get_problem_dirs_for_uid(caller_uid, dump_location);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "ChownProblemDir") == 0)
    {
        const gchar *problem_dir;
        int chown_res;

        g_variant_get(parameters, "(&s)", &problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        if (dir_accessible_by_uid(problem_dir, caller_uid)) //caller seems to be in group with access to this dir, so no action needed
        {
            VERB1 log("caller has access to the requested directory %s", problem_dir);
            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }

        if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            VERB1 log("not authorized");
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

        struct passwd *pwd = getpwuid(caller_uid);
        if (!pwd)
        {
            error_msg("UID %ld is not found in user database", (long)caller_uid);
            dd_close(dd);
            return;
        }

        errno = 0;
        struct stat statbuf;
        if (!(stat(problem_dir, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)))
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                  "org.freedesktop.problems.StatFailure",
                                  strerror(errno));
            dd_close(dd);
            return;
        }

        chown_res = chown(problem_dir, statbuf.st_uid, pwd->pw_gid);
        dd_init_next_file(dd);
        char *full_name;
        while (chown_res == 0 && dd_get_next_file(dd, /*short_name*/ NULL, &full_name))
        {
            VERB3 log("chowning %s", full_name);
            chown_res = chown(full_name, statbuf.st_uid, pwd->pw_gid);
            free(full_name);
        }

        if (chown_res != 0)
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.ChownError",
                                              strerror(errno));
        else
            g_dbus_method_invocation_return_value(invocation, NULL);

        dd_close(dd);
        return;
    }

    if (g_strcmp0(method_name, "GetInfo") == 0)
    {
        const gchar *problem_dir;
        g_variant_get(parameters, "(&s)", &problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        if (!dir_accessible_by_uid(problem_dir, caller_uid))
        {
            if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
            {
                VERB1 log("not authorized");
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

        GVariantBuilder *builder;
        builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add(builder, "{ss}", xstrdup(FILENAME_TIME), dd_load_text(dd, FILENAME_TIME));
        g_variant_builder_add(builder, "{ss}", xstrdup(FILENAME_REASON), dd_load_text(dd, FILENAME_REASON));
        char *not_reportable_reason = dd_load_text_ext(dd, FILENAME_NOT_REPORTABLE, 0
                                                       | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                                                       | DD_FAIL_QUIETLY_ENOENT
                                                       | DD_FAIL_QUIETLY_EACCES);
        if (not_reportable_reason)
            g_variant_builder_add(builder, "{ss}", xstrdup(FILENAME_NOT_REPORTABLE), not_reportable_reason);
        /* the source of the problem:
        * - first we try to load component, as we use it on Fedora
        */
        char *source = dd_load_text_ext(dd, FILENAME_COMPONENT, 0
                    | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                    | DD_FAIL_QUIETLY_ENOENT
                    | DD_FAIL_QUIETLY_EACCES
        );
        /* if we don't have component, we fallback to executable */
        if (!source)
        {
            source = dd_load_text_ext(dd, FILENAME_EXECUTABLE, 0
                    | DD_FAIL_QUIETLY_ENOENT
                    | DD_FAIL_QUIETLY_EACCES
            );
        }

        g_variant_builder_add(builder, "{ss}", xstrdup("source"), source);
        char *msg = dd_load_text_ext(dd, FILENAME_REPORTED_TO, 0
                    | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                    | DD_FAIL_QUIETLY_ENOENT
                    | DD_FAIL_QUIETLY_EACCES
        );
        if (msg)
            g_variant_builder_add(builder, "{ss}", xstrdup(FILENAME_REPORTED_TO), msg);

        dd_close(dd);

        GVariant *response = g_variant_new("(a{ss})", builder);
        g_variant_builder_unref(builder);

        free(msg);
        free(source);
        free(not_reportable_reason);

        VERB2 log("GetInfo: returning value for '%s'", problem_dir);
        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "DeleteProblem") == 0)
    {
        GList *problem_dirs = string_list_from_variant(parameters);

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;
            if (!allowed_problem_dir(dir_name))
            {
                return_InvalidProblemDir_error(invocation, dir_name);
                goto ret;
            }
        }

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;
            if (!dir_accessible_by_uid(dir_name, caller_uid))
            {
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
        const char *element;
        const char *value;
        long timestamp_from;
        long timestamp_to;
        bool all;

        g_variant_get(parameters, "(ssxxb)", &element, &value, &timestamp_from, &timestamp_to, &all);

        if (all && polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") == PolkitYes)
            caller_uid = 0;

        response = get_problem_dirs_for_element_in_time(caller_uid, element, value, timestamp_from,
                                                        timestamp_to, g_settings_dump_location);

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

static void on_name_lost (GDBusConnection *connection,
                  const gchar *name,
                      gpointer user_data)
{
    g_print(_("The name '%s' has been lost, please check if other "
              "service owning the name is not running.\n"), name);
    exit(1);
}

int main (int argc, char *argv[])
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
        OPT_INTEGER('t', NULL, &s_timeout, _("Exit after NUM seconds of inactivity")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    /* When dbus daemon starts us, it doesn't set PATH
     * (I saw it set only DBUS_STARTER_ADDRESS and DBUS_STARTER_BUS_TYPE).
     * In this case, set something sane:
     */
    const char *env_path = getenv("PATH");
    if (!env_path || !env_path[0])
        putenv((char*)"PATH=/usr/sbin:/usr/bin:/sbin:/bin");

    msg_prefix = "abrt-dbus"; /* for log(), error_msg() and such */

    if (!(opts & OPT_t))
        s_timeout = 120; //if the timeout is not set we default to 120sec

    if (getuid() != 0)
        error_msg_and_die(_("This program must be run as root."));

    g_type_init();


    /* We are lazy here - we don't want to manually provide
    * the introspection data structures - so we just build
    * them from XML.
    */
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_assert(introspection_data != NULL);

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                             ABRT_DBUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             NULL,
                             on_name_lost,
                             NULL,
                             NULL);

    load_abrt_conf();

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    VERB1 log("Cleaning up");

    g_bus_unown_name(owner_id);

    g_dbus_node_info_unref(introspection_data);

    free_abrt_conf_data();

    return 0;
}
