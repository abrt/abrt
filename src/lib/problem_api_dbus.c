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

#include "abrt_glib.h"
#include "internal_libabrt.h"

static GDBusProxy *get_dbus_proxy(void)
{
    static GDBusProxy *proxy;

    /* we cache it, so we can't free it! */
    if (proxy != NULL)
        return proxy;

    GError *error = NULL;
    proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         ABRT_DBUS_NAME,
                                         ABRT_DBUS_OBJECT,
                                         ABRT_DBUS_IFACE,
                                         NULL,
                                         &error);
    if (error)
    {
        error_msg(_("Can't connect to system DBus: %s"), error->message);
        g_error_free(error);
        /* proxy is NULL in this case */
    }
    return proxy;
}

int chown_dir_over_dbus(const char *problem_dir_path)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return 1;

    GError *error = NULL;
    g_dbus_proxy_call_sync(proxy,
                        "ChownProblemDir",
                        g_variant_new("(s)", problem_dir_path),
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        NULL,
                        &error);

    if (error)
    {
        error_msg(_("Can't chown '%s': %s"), problem_dir_path, error->message);
        g_error_free(error);
        return 1;
    }
    return 0;
}

int delete_problem_dirs_over_dbus(const GList *problem_dir_paths)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return 1;

    GVariant *parameters = variant_from_string_list(problem_dir_paths);

    GError *error = NULL;
    g_dbus_proxy_call_sync(proxy,
                    "DeleteProblem",
                    parameters,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    NULL,
                    &error);
//g_variant_unref(parameters); -- need this??? no?? why?

    if (error)
    {
        error_msg(_("Deleting problem directory failed: %s"), error->message);
        g_error_free(error);
        return 1;
    }
    return 0;
}

problem_data_t *get_problem_data_dbus(const char *problem_dir_path)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return NULL;

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(builder, "s", FILENAME_TIME          );
    g_variant_builder_add(builder, "s", FILENAME_REASON        );
    g_variant_builder_add(builder, "s", FILENAME_NOT_REPORTABLE);
    g_variant_builder_add(builder, "s", FILENAME_COMPONENT     );
    g_variant_builder_add(builder, "s", FILENAME_EXECUTABLE    );
    g_variant_builder_add(builder, "s", FILENAME_REPORTED_TO   );
    GVariant *params = g_variant_new("(sas)", problem_dir_path, builder);
    g_variant_builder_unref(builder);

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                            "GetInfo",
                                            params,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);

    if (error)
    {
        error_msg(_("Can't get problem data from abrt-dbus: %s"), error->message);
        g_error_free(error);
        return NULL;
    }

    problem_data_t *pd = problem_data_new();
    char *key, *val;
    GVariantIter *iter;
    g_variant_get(result, "(a{ss})", &iter);
    while (g_variant_iter_loop(iter, "{ss}", &key, &val))
    {
        problem_data_add_text_noteditable(pd, key, val);
    }
    g_variant_unref(result);
    return pd;
}

GList *get_problems_over_dbus(bool authorize)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return ERR_PTR;

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                    authorize ? "GetAllProblems" : "GetProblems",
                                    g_variant_new("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

    if (error)
    {
        error_msg(_("Can't get problem list from abrt-dbus: %s"), error->message);
        g_error_free(error);
        return ERR_PTR;
    }

    GList *list = NULL;
    if (result)
    {
        /* Fetch "as" from "(as)" */
        GVariant *array = g_variant_get_child_value(result, 0);
        list = string_list_from_variant(array);
        g_variant_unref(array);
        g_variant_unref(result);
    }

    return list;
}

problem_data_t *get_full_problem_data_over_dbus(const char *problem_dir_path)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return ERR_PTR;

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                    "GetProblemData",
                                    g_variant_new("(s)", problem_dir_path),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

    if (error)
    {
        error_msg(_("Can't get problem data from abrt-dbus: %s"), error->message);
        g_error_free(error);
        return ERR_PTR;
    }

    GVariantIter *iter = NULL;
    g_variant_get(result, "(a{s(its)})", &iter);

    gchar *name = NULL;
    gint flags;
    gulong size;
    gchar *value = NULL;

    problem_data_t *pd = problem_data_new();
    while (g_variant_iter_loop(iter, "{&s(it&s)}", &name, &flags, &size, &value))
        problem_data_add_ext(pd, name, value, flags, size);

    problem_data_add(pd, CD_DUMPDIR, problem_dir_path,
            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);

    g_variant_unref(result);

    return pd;
}

int test_exist_over_dbus(const char *problem_id, const char *element_name)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return -1;

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                            "TestElementExists",
                                            g_variant_new("(ss)", problem_id, element_name),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);

    if (error)
    {
        error_msg(_("Can't test whether the element exists over abrt-dbus: %s"), error->message);
        g_error_free(error);
        return -1;
    }

    gboolean retval;
    g_variant_get(result, "(b)", &retval);
    g_variant_unref(result);

    return retval;
}

char *load_text_over_dbus(const char *problem_id, const char *element_name)
{
    INITIALIZE_LIBABRT();

    GDBusProxy *proxy = get_dbus_proxy();
    if (!proxy)
        return ERR_PTR;

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(builder, "s", element_name);
    GVariant *params = g_variant_new("(sas)", problem_id, builder);
    g_variant_builder_unref(builder);

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                            "GetInfo",
                                            params,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);

    if (error)
    {
        error_msg(_("Can't get problem data from abrt-dbus: %s"), error->message);
        g_error_free(error);
        return ERR_PTR;
    }

    GVariant *values = g_variant_get_child_value(result, 0);
    g_variant_unref(result);

    char *retval = NULL;
    if (g_variant_n_children(values) == 1)
    {
        GVariant *contents = g_variant_get_child_value(values, 0);
        gchar *key;
        g_variant_get(contents, "{&ss}", &key, &retval);
    }

    g_variant_unref(values);
    return retval;
}
