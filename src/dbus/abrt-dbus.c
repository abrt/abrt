#include <gio/gio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "libabrt.h"
#include "abrt-polkit.h"
#include "abrt-dbus.h"

/* how long should we wait from the last request */
#define TIME_TO_DIE 5

GMainLoop *loop;
guint g_timeout;

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
  "    <method name='Quit' />"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

/* forward */
static gboolean on_timeout_cb(gpointer user_data);

static void reset_timeout()
{
    //FIXME: reset timer on every call
    if (g_timeout > 0)
    {
        VERB2 log("Removing timeout\n");
        g_source_remove(g_timeout);
    }
    VERB2 log("Setting a new timeout\n");
    g_timeout = g_timeout_add_seconds(TIME_TO_DIE, on_timeout_cb, NULL);
}

uid_t get_caller_uid(GDBusConnection *connection, const char *caller, GError *error)
{
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

    if (result == NULL) {
        /* this value shouldn't be used, beacuse error is set,
         * but return 99 -> nobody just to be sure
         */
        return 99;
    }

    g_variant_get(result, "(u)", &caller_uid);
    g_variant_unref(result);

    VERB2 log("Caller uid: %i\n", caller_uid);
    return caller_uid;
}

bool uid_in_group(uid_t uid, gid_t gid)
{
    char **tmp;
    struct passwd *pwd = getpwuid(uid);
    if (pwd && (pwd->pw_gid == gid))
    {
        return TRUE;
    }

    struct group *grp = getgrgid(gid);
    if (pwd && grp && grp->gr_mem)
    {
        for (tmp = grp->gr_mem; *tmp != NULL; tmp++)
        {
            if (g_strcmp0(*tmp, pwd->pw_name) == 0)
            {
                VERB3 log("user %s belongs to group: %s\n",  pwd->pw_name, grp->gr_name);
                return TRUE;
            }
        }
    }
    VERB2 log("WARN: user %s DOESN'T belong to group: %s\n",  pwd->pw_name, grp->gr_name);
    return FALSE;
}

static GList* scan_directory(const char *path, uid_t caller_uid)
{
    GList *list = NULL;

    DIR *dp = opendir(path);
    if (!dp)
    {
        /* We don't want to yell if, say, $HOME/.abrt/spool doesn't exist */
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
                list = g_list_prepend(list, full_name);
                full_name = NULL;
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

GVariant *get_problem_dirs_for_uid(uid_t uid, const char *dump_location)
{
    GList *dirs = scan_directory(dump_location, uid);

    GVariantBuilder *builder;
    builder = g_variant_builder_new(G_VARIANT_TYPE ("as"));
    while (dirs)
    {
        g_variant_builder_add(builder, "s", (char*)dirs->data);
        free(dirs->data);
        dirs = g_list_delete_link(dirs, dirs);
    }

    GVariant *response = g_variant_new("(as)", builder);
    g_variant_builder_unref(builder);

    return response;
}

static void
handle_method_call(GDBusConnection       *connection,
                    const gchar           *caller,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    reset_timeout();
    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        GError *error = NULL;
        GVariant *response;
        uid_t caller_uid;


        caller_uid = get_caller_uid(connection, caller, error);

        if (error)
            g_warning("Could not get unix user: %s", error->message);
            //die!
        else if (caller_uid == 99)
            //die
            g_warning("Could not get unix user, using 99 - nobody");

        //read config here!

        const gchar *dump_location;
        g_variant_get(parameters, "(&s)", &dump_location);

        //if (!g_settings_dump_location)
        //    g_settings_dump_location = (char*)"/var/spool/abrt";

        response = get_problem_dirs_for_uid(caller_uid, dump_location);

        g_dbus_method_invocation_return_value(invocation, response);
        //I was told that g_dbus_method frees the response
        //g_variant_unref(response);
    }

    else if (g_strcmp0(method_name, "GetAllProblems") == 0)
    {
        GVariant *response;
        GError *error = NULL;
        uid_t caller_uid;


        const gchar *dump_location;
        g_variant_get(parameters, "(&s)", &dump_location);

        caller_uid = get_caller_uid(connection, caller, error);

        if (error)
        {
            g_warning("Could not get unix user: %s", error->message);
            //die!
            g_error_free(error);
        }
        else if (caller_uid == 99)
            //die
            g_warning("Could not get unix user, using 99 - nobody");

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
    }

    else if (g_strcmp0(method_name, "ChownProblemDir") == 0)
    {
        VERB2 log("ChownProblemDir");

        GError *error = NULL;
        uid_t caller_uid;
        const gchar *problem_dir;
        struct passwd *pwd;
        int chown_res;
        gchar *error_msg;

        g_variant_get(parameters, "(&s)", &problem_dir);

        //FIXME: check if it's problem_dir and refuse to operate on it if it's not
        struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            error_msg = g_strdup_printf(_("%s is not a valid problem dir"), problem_dir);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidProblemDir",
                                                  error_msg);
            free(error_msg);
            return;
        }
        dd_close(dd);

        caller_uid = get_caller_uid(connection, caller, error);

        g_print("out\n");

        if (error)
        {
            g_dbus_method_invocation_return_gerror(invocation, error);
            g_error_free(error);
        }

        if (caller_uid == 99)
        {
            error_msg = g_strdup_printf("Could not get unix user for caller %s", caller);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                      "org.freedesktop.problems.InvalidProblemDir",
                                                  error_msg);
            free(error_msg);
            return;
        }

        struct stat statbuf;
        errno = 0;
        if (stat(problem_dir, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
        {
            if (caller_uid == 0 || uid_in_group(caller_uid, statbuf.st_gid)) //caller seems to be in group with access to this dir, so no action needed
            {
                //return ok
                VERB1 log("caller has access to the requested directory %s\n", problem_dir);
                g_dbus_method_invocation_return_value(invocation, NULL);
                return;
                //free something?
            }

        }
        else
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.StatFailure",
                                                      strerror(errno));
            return;
        }

        if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            VERB1 log("not authorized");
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              "Not Authorized");
            return;
        }

        pwd = getpwuid(caller_uid);
        if (pwd)
        {
            errno = 0;
            chown_res = chown(problem_dir, statbuf.st_uid, pwd->pw_gid);
            if (chown_res != 0)
                g_dbus_method_invocation_return_dbus_error(invocation,
                                                  "org.freedesktop.problems.ChownError",
                                                  strerror(errno));

            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }
    }

    else if (g_strcmp0(method_name, "GetInfo") == 0)
    {
        VERB1 log("GetInfo\n");

        const gchar *problem_dir;
        g_variant_get(parameters, "(&s)", &problem_dir);

        GVariantBuilder *builder;

        struct dump_dir *dd = dd_opendir(problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            char *error_msg = g_strdup_printf(_("%s is not a valid problem directory"), problem_dir);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                  "org.freedesktop.problems.GetInfo",
                                                  error_msg);
            free(error_msg);
        }

        builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add (builder, "{ss}", g_strdup(FILENAME_TIME), dd_load_text(dd, FILENAME_TIME));

        dd_close(dd);

        GVariant *response = g_variant_new("(a{ss})", builder);
        g_variant_builder_unref(builder);

        g_dbus_method_invocation_return_value(invocation, response);
    }

    else if (g_strcmp0(method_name, "Quit") == 0)
    {
        VERB1 log("Quit\n");

        g_dbus_method_invocation_return_value(invocation, NULL);

        g_main_loop_quit(loop);

    }
}

static gboolean
on_timeout_cb(gpointer user_data)
{
    g_print("timeout, would die, but we're still in debug mode\n");
    //g_main_loop_quit(loop);
    return TRUE;
}

/* for now */
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL,
};

static void
on_bus_acquired(GDBusConnection *connection,
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

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint owner_id;

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
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);

  g_print("Finishing server\n");

  g_bus_unown_name(owner_id);

  g_dbus_node_info_unref(introspection_data);

  return 0;
}

