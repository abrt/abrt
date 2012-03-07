#include <gio/gio.h>
#include <stdlib.h>

#ifdef G_OS_UNIX
#include <gio/gunixfdlist.h>
/* For STDOUT_FILENO */
#include <unistd.h>
#endif

GMainLoop *loop;

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.problems'>"
  "    <method name='GetProblems'>"
  "      <arg type='as' name='response' direction='out'/>"
  "    </method>"
  "    <method name='GetInfo'>"
  "      <arg type='s' name='problem_dir' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='Quit' />"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

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
    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        g_print("GetProblems\n");

        GVariantBuilder *builder;
        builder = g_variant_builder_new(G_VARIANT_TYPE ("as"));
        //for i in dirs
        g_variant_builder_add(builder, "s", "ahoj");
        g_variant_builder_add(builder, "s", "svete");
        GVariant *response = g_variant_new("(as)", builder);
        g_variant_builder_unref(builder);

        g_dbus_method_invocation_return_value(invocation, response);
        g_variant_unref(response);
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

static gchar *_global_title = NULL;

static gboolean swap_a_and_b = FALSE;

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
  GVariant *ret;

  ret = NULL;
  if (g_strcmp0 (property_name, "FluxCapicitorName") == 0)
    {
      ret = g_variant_new_string ("DeLorean");
    }
  else if (g_strcmp0 (property_name, "Title") == 0)
    {
      if (_global_title == NULL)
        _global_title = g_strdup ("Back To C!");
      ret = g_variant_new_string (_global_title);
    }
  else if (g_strcmp0 (property_name, "ReadingAlwaysThrowsError") == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Hello %s. I thought I said reading this property "
                   "always results in an error. kthxbye",
                   sender);
    }
  else if (g_strcmp0 (property_name, "WritingAlwaysThrowsError") == 0)
    {
      ret = g_variant_new_string ("There's no home like home");
    }
  else if (g_strcmp0 (property_name, "Foo") == 0)
    {
      ret = g_variant_new_string (swap_a_and_b ? "Tock" : "Tick");
    }
  else if (g_strcmp0 (property_name, "Bar") == 0)
    {
      ret = g_variant_new_string (swap_a_and_b ? "Tick" : "Tock");
    }

  return ret;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
  if (g_strcmp0 (property_name, "Title") == 0)
    {
      if (g_strcmp0 (_global_title, g_variant_get_string (value, NULL)) != 0)
        {
          GVariantBuilder *builder;
          GError *local_error;

          g_free (_global_title);
          _global_title = g_variant_dup_string (value, NULL);

          local_error = NULL;
          builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
          g_variant_builder_add (builder,
                                 "{sv}",
                                 "Title",
                                 g_variant_new_string (_global_title));
          g_dbus_connection_emit_signal (connection,
                                         NULL,
                                         object_path,
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         g_variant_new ("(sa{sv}as)",
                                                        interface_name,
                                                        builder,
                                                        NULL),
                                         &local_error);
          g_assert_no_error (local_error);
        }
    }
  else if (g_strcmp0 (property_name, "ReadingAlwaysThrowsError") == 0)
    {
      /* do nothing - they can't read it after all! */
    }
  else if (g_strcmp0 (property_name, "WritingAlwaysThrowsError") == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Hello AGAIN %s. I thought I said writing this property "
                   "always results in an error. kthxbye",
                   sender);
    }

  return *error == NULL;
}


/* for now */
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  handle_set_property
};

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_timeout_cb (gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);
  GVariantBuilder *builder;
  GVariantBuilder *invalidated_builder;
  GError *error;

  swap_a_and_b = !swap_a_and_b;

  error = NULL;
  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_variant_builder_add (builder,
                         "{sv}",
                         "Foo",
                         g_variant_new_string (swap_a_and_b ? "Tock" : "Tick"));
  g_variant_builder_add (builder,
                         "{sv}",
                         "Bar",
                         g_variant_new_string (swap_a_and_b ? "Tick" : "Tock"));
  g_dbus_connection_emit_signal (connection,
                                 NULL,
                                 "/org/gtk/GDBus/TestObject",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)",
                                                "org.gtk.GDBus.TestInterface",
                                                builder,
                                                invalidated_builder),
                                 &error);
  g_assert_no_error (error);


  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org.freedesktop.problems",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);

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

  g_type_init ();

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
  g_assert(introspection_data != NULL);

  owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                             "org.freedesktop.problems",
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

