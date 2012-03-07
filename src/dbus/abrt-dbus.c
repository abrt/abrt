#include <gio/gio.h>
#include <stdlib.h>
#include "libabrt.h"
#include "abrt-polkit.h"

/* how long should we wait from the last request */
#define TIME_TO_DIE 5

GMainLoop *loop;
guint g_timeout;

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='com.redhat.abrt'>"
  "    <method name='GetProblems'>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetAllProblems'>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetInfo'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
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
        g_print("Removing timeout\n");
        g_source_remove(g_timeout);
    }
    g_print("setting a new timeout\n");
    g_timeout = g_timeout_add_seconds(TIME_TO_DIE, on_timeout_cb, NULL);
}

static GList* scan_directory(const char *path)
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
            if (dd)
            {
                list = g_list_prepend(list, full_name);
                full_name = NULL;
                dd_close(dd);
            }
        }
        free(full_name);
    }
    closedir(dp);

    /* Why reverse?
     * Because N*prepend+reverse is faster than N*append
     */
    return g_list_reverse(list);
}

static void
handle_method_call(GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    reset_timeout();
    g_print("%s\n", sender);
    //g_print("%i\n", g_credentials_get_unix_user(g_dbus_connection_get_peer_credentials(connection), NULL));
    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        g_print("GetProblems\n");

        //g_settings_dump_location comes from libabrt.h
        g_print("%p\n", g_settings_dump_location);
        if (!g_settings_dump_location)
            g_settings_dump_location = (char*)"/var/spool/abrt";
        GList *dirs = scan_directory(g_settings_dump_location);

        GVariantBuilder *builder;
        builder = g_variant_builder_new(G_VARIANT_TYPE ("as"));
        //for i in dirs
        while (dirs)
        {
            g_variant_builder_add(builder, "s", (char*)dirs->data);
            free(dirs->data);
            dirs = g_list_delete_link(dirs, dirs);
        }

        GVariant *response = g_variant_new("(as)", builder);
        g_variant_builder_unref(builder);

        g_dbus_method_invocation_return_value(invocation, response);
        g_variant_unref(response);
    }
    if (g_strcmp0(method_name, "GetAllProblems") == 0)
    {
        gchar *response;
        if (polkit_check_authorization_dname(sender, "org.freedesktop.problems.getall") != PolkitYes)
            response = g_strdup_printf("Not authorized");
        else
            response = g_strdup_printf("Authorized");

        GVariant *gv_response = g_variant_new("(s)", response);
        g_dbus_method_invocation_return_value(invocation, gv_response);
        g_variant_unref(gv_response);
    }
    if (g_strcmp0(method_name, "GetInfo") == 0)
    {
        g_print("GetInfo\n");

        const gchar *problem_dir;
        g_variant_get(parameters, "(&s)", &problem_dir);
        gchar *response = g_strdup_printf("You've requested dir: '%s'. ", problem_dir);

        g_dbus_method_invocation_return_value(invocation,
                                        g_variant_new("(s)", response));
    }
    if (g_strcmp0(method_name, "Quit") == 0)
    {
        g_print("Quit\n");

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
                                                       "/com/redhat/abrt",
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
                             "com.redhat.abrt",
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

