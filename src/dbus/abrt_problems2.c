/*
  Copyright (C) 2015  ABRT team

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
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "abrt_problems2_service.h"
#include "libabrt.h"
#include <gio/gio.h>

static GMainLoop *g_loop;
static int g_timeout_value = 10;

static void on_bus_acquired(GDBusConnection *connection,
                            const gchar     *name,
                            gpointer         user_data)
{
    GError *error = NULL;

    int r = abrt_p2_service_register_objects(ABRT_P2_SERVICE(user_data), connection, &error);
    if (r == -EALREADY)
        return;

    error_msg("Failed to register Problems2 Objects: %s", error->message);
    g_error_free(error);

    g_main_loop_quit(g_loop);
}

static void on_name_acquired(GDBusConnection *connection,
                             const gchar     *name,
                             gpointer         user_data)
{
    log_debug("Acquired the name '%s' on the system bus", name);
}

static void on_name_lost(GDBusConnection *connection,
                         const gchar     *name,
                         gpointer         user_data)
{
    log_warning("The name '%s' has been lost, please check if other "
              "service owning the name is not running.\n", name);

    g_main_loop_quit(g_loop);
}

void quit_loop(int signo)
{
    g_main_loop_quit(g_loop);
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

    glib_init();
    abrt_init(argv);
    load_abrt_conf();

    const char *program_usage_string = "& [options]";

    enum {
        OPT_v = 1 << 0,
        OPT_t = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_INTEGER('t', NULL, &g_timeout_value, "Exit after NUM seconds of inactivity"),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = "abrt-problems2"; /* for log_warning(), error_msg() and such */

    if (getuid() != 0)
        error_msg_and_die("This program must be run as root.");

    GError *error = NULL;
    AbrtP2Service *service = abrt_p2_service_new(&error);
    if (service == NULL)
    {
        error_msg_and_die("Failed to initialize ABRT Problems2 service: %s",
                error->message);
    }

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                              ABRT_P2_BUS,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              service,
                              g_object_unref);


    g_loop = g_main_loop_new(NULL, FALSE);

    signal(SIGABRT, quit_loop);

    g_main_loop_run(g_loop);
    g_main_loop_unref(g_loop);

    log_notice("Cleaning up");

    if (owner_id > 0)
        g_bus_unown_name(owner_id);

    free_abrt_conf_data();

    return 0;
}

