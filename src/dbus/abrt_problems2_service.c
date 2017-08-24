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

#include <polkit/polkit.h>

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include "libabrt.h"
#include "problem_api.h"
#include "abrt_problems2_task_new_problem.h"
#include "abrt_problems2_task.h"
#include "abrt_problems2_generated_interfaces.h"
#include "abrt_problems2_service.h"
#include "abrt_problems2_session.h"
#include "abrt_problems2_entry.h"

#include <dbus/dbus.h>

/* Shared polkit authority */
PolkitAuthority *g_polkit_authority;
int g_polkit_authority_refs;

/*
 * DBus object type
 */
struct problems2_object_type
{
    GDBusNodeInfo *node;
    GDBusInterfaceInfo *iface;
    GDBusInterfaceVTable *vtable;
    GHashTable *objects;
};

static int problems2_object_type_init(struct problems2_object_type *type,
            const char *xml_node,
            GDBusInterfaceVTable *vtable)
{
    GError *local_error = NULL;
    type->node = g_dbus_node_info_new_for_xml(xml_node, &local_error);
    if (local_error != NULL)
    {
        log_info("Failed to parse XML interface file: %s", local_error->message);
        g_error_free(local_error);
        return -1;
    }

    type->iface = type->node->interfaces[0];
    type->vtable = vtable;
    type->objects = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    return 0;
}

static void problems2_object_type_destroy(struct problems2_object_type *type)
{
    if (type->objects != NULL)
    {
        g_hash_table_destroy(type->objects);
        type->objects = NULL;
    }

    if (type->node != NULL)
    {
        g_dbus_node_info_unref(type->node);
        type->node = NULL;
    }
}

#if 0
/* Debuging function */
static void problems2_object_type_print_all_objects(struct problems2_object_type *type,
            const char *prefix)
{
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, type->objects);

    const char *p;
    AbrtP2Object *obj;
    while(g_hash_table_iter_next(&iter, (gpointer)&p, (gpointer)&obj))
        log_warning("%s: '%s' : %p", prefix, p, obj);
}
#endif

static AbrtP2Object *problems2_object_type_get_object(struct problems2_object_type *type,
            const char *path)
{
    AbrtP2Object *obj = g_hash_table_lookup(type->objects, path);
    return obj;
}

static GList *problems2_object_type_get_all_objects(struct problems2_object_type *type)
{
    GList *objects = g_hash_table_get_values(type->objects);
    return objects;
}

/*
 * User details
 */
struct user_info
{
    GList *sessions;
    long unsigned problems;
    unsigned new_problems;
    time_t new_problem_last;
};

static struct user_info *user_info_new(void)
{
    struct user_info *user = xzalloc(sizeof(*user));
    return user;
}

static void user_info_free(struct user_info *info)
{
    if (info == NULL)
        return;

    g_list_free(info->sessions);
    info->sessions = (void *)0xDAEDBEEF;

    free(info);
}

/*
 * AbrtP2Service GObject Type
 */
enum {
    SERVICE_SIGNALS_NEW_CLIENT_CONNECTED,
    SERVICE_SIGNALS_ALL_CLIENTS_DISCONNECTED,
    SERVICE_SIGNALS_NUM,
};
static guint service_signals[SERVICE_SIGNALS_NUM] = { 0 };

typedef struct
{
    GDBusConnection *p2srv_dbus;
    GDBusProxy      *p2srv_proxy_dbus;
    GHashTable      *p2srv_connected_users;
    PolkitAuthority *p2srv_pk_authority;

    struct problems2_object_type p2srv_p2_type;
    struct problems2_object_type p2srv_p2_entry_type;
    struct problems2_object_type p2srv_p2_session_type;
    struct problems2_object_type p2srv_p2_task_type;

    long     p2srv_max_message_size;
    long     p2srv_max_message_unix_fds;
    unsigned p2srv_limit_clients;
    unsigned p2srv_limit_elements;
    off_t    p2srv_limit_data_size;
    unsigned p2srv_limit_user_problems;
    unsigned p2srv_limit_new_problem_throttling_magnitude;
    unsigned p2srv_limit_new_problems_batch;

    AbrtP2Object *p2srv_p2_object;
} AbrtP2ServicePrivate;

struct _AbrtP2Service
{
    GObject parent_instance;
    AbrtP2ServicePrivate *pv;
};

G_DEFINE_TYPE_WITH_PRIVATE(AbrtP2Service, abrt_p2_service, G_TYPE_OBJECT)

/*
 * Private functions
 */
static struct user_info *abrt_p2_service_user_lookup(AbrtP2Service *service,
            uid_t uid);

static struct user_info *abrt_p2_service_user_insert(AbrtP2Service *service,
            uid_t uid,
            struct user_info *user);

static struct user_info *abrt_p2_service_user_new(AbrtP2Service *service,
            uid_t uid);

static GDBusConnection *abrt_p2_service_dbus(AbrtP2Service *service);

/*
 * DBus object
 */
struct _AbrtP2Object
{
    AbrtP2Service *p2o_service;
    struct problems2_object_type *p2o_type;
    char *p2o_path;
    guint p2o_regid;
    void *node;
    void (*destructor)(AbrtP2Object *);
};

static void abrt_p2_object_free(AbrtP2Object *obj)
{
    if (obj == NULL)
        return;

    /* remove the destroyed object before destructing it */
    g_hash_table_remove(obj->p2o_type->objects, obj->p2o_path);

    if (obj->destructor)
        obj->destructor(obj);

    obj->node = (void *)0xDEADBEAF;
    obj->destructor = (void *)0xDEADBEAF;

    free(obj->p2o_path);
    obj->p2o_path = (void *)0xDEADBEAF;

    obj->p2o_regid = (guint)-1;

    obj->p2o_service = NULL;

    free(obj);
}

static AbrtP2Service *abrt_p2_object_service(AbrtP2Object *object)
{
    return object->p2o_service;
}

void *abrt_p2_object_get_node(AbrtP2Object *object)
{
    return object->node;
}

void abrt_p2_object_destroy(AbrtP2Object *object)
{
    log_debug("Unregistering object: %s", object->p2o_path);

    g_dbus_connection_unregister_object(abrt_p2_service_dbus(object->p2o_service),
                                        object->p2o_regid);
}

static void abrt_p2_object_emit_signal_with_destination(AbrtP2Object *object,
            const char *member,
            GVariant *parameters,
            const char *destination)
{
    GDBusMessage *message = g_dbus_message_new_signal(object->p2o_path,
                                                      object->p2o_type->iface->name,
                                                      member);
    if (destination != NULL)
        g_dbus_message_set_destination(message, destination);

    g_dbus_message_set_sender(message, ABRT_P2_BUS);
    g_dbus_message_set_body(message, parameters);

    if (g_verbose > 2)
    {
        gchar *pstr = g_variant_print(parameters, TRUE);
        log_debug("Emitting signal '%s' : (%s)", member, pstr);
        g_free(pstr);
    }

    GError *error = NULL;
    g_dbus_connection_send_message(abrt_p2_service_dbus(object->p2o_service),
                                   message,
                                   G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                   NULL,
                                   &error);
    g_object_unref(message);
    if (error != NULL)
    {
        error_msg("Failed to emit signal '%s': %s", member, error->message);
        g_free(error);
    }
}

static AbrtP2Object *abrt_p2_object_new(AbrtP2Service *service,
            struct problems2_object_type *type,
            char *path,
            void *node,
            void (*destructor)(AbrtP2Object *),
            GError **error)
{
    AbrtP2Object *obj = NULL;
    obj = xzalloc(sizeof(*obj));
    obj->p2o_path = path;
    obj->node = node;
    obj->destructor = destructor;
    obj->p2o_type = type;
    obj->p2o_service = service;

    /* Register the interface parsed from a XML file */
    log_debug("Registering PATH %s iface %s", path, type->iface->name);
    const guint registration_id = g_dbus_connection_register_object(abrt_p2_service_dbus(service),
                                                                    path,
                                                                    type->iface,
                                                                    type->vtable,
                                                                    obj,
                                                                    (GDestroyNotify)abrt_p2_object_free,
                                                                    error);

    if (registration_id == 0)
    {
        g_prefix_error(error, "Failed to register path:'%s', interface: %s: ",
                       path,
                       type->iface->name);

        abrt_p2_object_free(obj);

        return NULL;
    }

    log_debug("Registered object: %d", registration_id);

    obj->p2o_regid = registration_id;

    g_hash_table_insert(type->objects, path, obj);

    return obj;
}

const char *abrt_p2_object_path(AbrtP2Object *obj)
{
    return obj->p2o_path;
}

/*
 * /org/freedesktop/Problems2/Session/XYZ
 */
static void session_object_dbus_method_call(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant    *parameters,
            GDBusMethodInvocation *invocation,
            gpointer    user_data)
{
    log_debug("Problems2.Sessions method : %s", method_name);

    /* Check sanity */
    if (strcmp(interface_name, ABRT_P2_NS_MEMBER("Session")) != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return;
    }

    GError *error = NULL;

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t caller_uid = abrt_p2_service_caller_real_uid(service, caller, &error);
    if (caller_uid == (uid_t)-1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    AbrtP2Session *session = abrt_p2_object_get_node(user_data);
    if (abrt_p2_session_check_sanity(session, caller, caller_uid, &error) != 0)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    if (strcmp("Authorize", method_name) == 0)
    {
        GVariant *details = g_variant_get_child_value(parameters, 0);
        struct user_info *user = abrt_p2_service_user_lookup(service, caller_uid);
        const gint32 retval = abrt_p2_session_authorize(session,
                                                        details,
                                                        user->sessions,
                                                        &error);
        g_variant_unref(details);

        if (retval < 0)
        {
            g_prefix_error(&error, "Failed to authorize Session: ");
            g_dbus_method_invocation_return_gerror(invocation, error);
            g_error_free(error);
            return;
        }

        GVariant *response = g_variant_new("(i)", retval);
        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (strcmp("GenerateToken", method_name) == 0)
    {
        guint duration = 0;
        g_variant_get(parameters, "(u)", &duration);
        const char *token = abrt_p2_session_generate_token(session, duration, &error);
        if (token == NULL)
        {
            g_prefix_error(&error, "Cannot generate token: ");
            g_dbus_method_invocation_return_gerror(invocation, error);
            g_error_free(error);
            return;
        }

        GVariant *response = g_variant_new("(s)", token);
        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (strcmp("RevokeToken", method_name) == 0)
    {
        GVariant *token = g_variant_get_child_value(parameters, 0);
        if (abrt_p2_session_revoke_token(session, g_variant_get_string(token, NULL)) != 0)
            log_warning("Could not remove Session Token because it was already gone.");
        return;
    }

    if (strcmp("RevokeAuthorization", method_name) == 0)
    {
        abrt_p2_session_revoke_authorization(session);
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    error_msg("BUG: org.freedesktop.Problems2.Session does not have method: %s",
              method_name);
}

static GVariant *session_object_dbus_get_property(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GError      **error,
            gpointer    user_data)
{
    log_debug("Problems2.Sessions get property : %s", property_name);

    if (strcmp(interface_name, "org.freedesktop.Problems2.Session") != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return NULL;
    }

    if (strcmp("IsAuthorized", property_name))
    {
        error_msg("Unsupported property %s", property_name);
        return NULL;
    }

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t caller_uid = abrt_p2_service_caller_real_uid(service, caller, error);
    if (caller_uid == (uid_t)-1)
        return NULL;

    AbrtP2Session *node = abrt_p2_object_get_node(user_data);
    if (abrt_p2_session_check_sanity(node, caller, caller_uid, error) != 0)
        return NULL;

    return g_variant_new_boolean(abrt_p2_session_is_authorized(node));
}

static void session_object_destructor(AbrtP2Object *obj)
{
    AbrtP2Session *session = (AbrtP2Session *)obj->node;

    uid_t uid = abrt_p2_session_uid(session);

    struct user_info *user = abrt_p2_service_user_lookup(obj->p2o_service, uid);

    if (user->sessions == NULL)
    {
        error_msg("BUG: destructing session object for user who does not have session opened");
        abort();
    }

    abrt_p2_session_clean_tasks(session);
    abrt_p2_session_revoke_authorization(session);

    user->sessions = g_list_remove(user->sessions, session);

    const guint size = g_hash_table_size(obj->p2o_type->objects);
    if (size == 0)
    {
        g_signal_emit(obj->p2o_service,
                      service_signals[SERVICE_SIGNALS_ALL_CLIENTS_DISCONNECTED],
                      0/*details*/);
    }

    g_object_unref(session);
}

static void session_object_on_authorization_changed(AbrtP2Session *session,
            gint32 status,
            gpointer object)
{
    GVariant *params = g_variant_new("(i)", status);
    const char *session_bus_address = abrt_p2_session_caller(session);
    abrt_p2_object_emit_signal_with_destination(object,
                                                "AuthorizationChanged",
                                                params,
                                                session_bus_address);
}

static AbrtP2Object *session_object_register(AbrtP2Service *service,
            char *path,
            const char *caller,
            uid_t caller_uid,
            GError **error)
{
    struct user_info *user = abrt_p2_service_user_lookup(service, caller_uid);

    const unsigned client_limits = abrt_p2_service_user_clients_limit(service, caller_uid);
    if (user != NULL && g_list_length(user->sessions) >= client_limits)
    {
        log_warning("User %lu reached the limit of opened sessions (%d)",
                    (long unsigned)caller_uid,
                    client_limits);

        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Too many sessions opened");

        free(path);
        return NULL;
    }

    char *dup_caller = xstrdup(caller);

    AbrtP2Session *session = abrt_p2_session_new(dup_caller, caller_uid);

    AbrtP2Object *obj = abrt_p2_object_new(service,
                                           &(service->pv->p2srv_p2_session_type),
                                           path,
                                           session,
                                           session_object_destructor,
                                           error);

    if (obj == NULL)
    {
        g_prefix_error(error, "Failed to register Session object for caller '%s': ", caller);
        return NULL;
    }

    g_signal_connect(session, "authorization-changed", G_CALLBACK(session_object_on_authorization_changed), obj);

    if (user == NULL)
        user = abrt_p2_service_user_new(service, caller_uid);

    user->sessions = g_list_prepend(user->sessions, session);

    g_signal_emit(service,
                  service_signals[SERVICE_SIGNALS_NEW_CLIENT_CONNECTED],
                  0/*details*/);

    return obj;
}

static char *session_object_caller_to_path(const char *caller)
{
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    str_to_sha1str(hash_str, caller);
    return xasprintf(ABRT_P2_PATH"/Session/%s", hash_str);
}

static AbrtP2Object *abrt_p2_service_get_session_for_caller(
            AbrtP2Service *service,
            const char *caller,
            uid_t caller_uid,
            GError **error)
{
    char *session_path = session_object_caller_to_path(caller);

    AbrtP2Object *obj = problems2_object_type_get_object(&(service->pv->p2srv_p2_session_type),
                                                         session_path);
    if (obj == NULL)
    {
        log_debug("Caller does not have Session: %s", caller);
        return session_object_register(service,
                                       session_path,
                                       caller,
                                       caller_uid,
                                       error);
    }

    free(session_path);

    AbrtP2Session *session = abrt_p2_object_get_node(obj);
    if (abrt_p2_session_check_sanity(session, caller, caller_uid, error) != 0)
    {
        log_debug("Cannot return session because the existing one did not pass sanity check.");
        return NULL;
    }

    return obj;
}

const char *abrt_p2_service_session_path(AbrtP2Service *service,
            const char *caller,
            GError **error)
{
    uid_t caller_uid = abrt_p2_service_caller_real_uid(service, caller, error);
    if (caller_uid == (uid_t)-1)
        return NULL;

    AbrtP2Object *obj = abrt_p2_service_get_session_for_caller(service,
                                                               caller,
                                                               caller_uid,
                                                               error);

    return obj == NULL ? NULL : obj->p2o_path;
}

static uid_t abrt_p2_service_get_session_uid(AbrtP2Service *service,
            AbrtP2Session *session)
{
    return abrt_p2_session_is_authorized(session)
            ? 0
            : abrt_p2_session_uid(session);
}

uid_t abrt_p2_service_caller_uid(AbrtP2Service *service,
            const char *caller,
            GError **error)
{
    uid_t caller_uid = abrt_p2_service_caller_real_uid(service, caller, error);
    if (caller_uid == (uid_t)-1)
        return (uid_t)-1;

    AbrtP2Object *obj = abrt_p2_service_get_session_for_caller(service,
                                                               caller,
                                                               caller_uid,
                                                               error);
    if (obj == NULL)
        return (uid_t) -1;

    AbrtP2Session *session = abrt_p2_object_get_node(obj);
    return abrt_p2_service_get_session_uid(service, session);
}

uid_t abrt_p2_service_caller_real_uid(AbrtP2Service *service,
            const char *caller,
            GError **error)
{
    guint caller_uid;

    if (service->pv->p2srv_proxy_dbus == NULL)
        return (uid_t) -1;

    GVariant *result = g_dbus_proxy_call_sync(service->pv->p2srv_proxy_dbus,
                                              "GetConnectionUnixUser",
                                              g_variant_new ("(s)",
                                                             caller),
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              error);

    if (result == NULL)
        return (uid_t) -1;

    g_variant_get(result, "(u)", &caller_uid);
    g_variant_unref(result);

    log_info("Caller uid: %i", caller_uid);
    return caller_uid;
}

/*
 * /org/freedesktop/Problems2/Entry/XYZ
 */
void abrt_p2_service_notify_entry_object(AbrtP2Service *service,
            AbrtP2Object *obj,
            GError **error)
{
    AbrtP2Entry *entry = abrt_p2_object_get_node(obj);
    uid_t owner_uid = abrt_p2_entry_get_owner(entry, error);

    if (owner_uid >= 0)
    {
        GList *session_objects = problems2_object_type_get_all_objects(
                                         &service->pv->p2srv_p2_session_type);

        for (GList *iter = session_objects; iter != NULL; iter = g_list_next(iter))
        {
            AbrtP2Session *session = abrt_p2_object_get_node(iter->data);
            const uid_t session_uid = abrt_p2_service_get_session_uid(service,
                                                                      session);

            const char *session_bus_address = abrt_p2_session_caller(session);

            if ( 0 != abrt_p2_entry_accessible_by_uid(entry, session_uid, NULL))
            {
                log_debug("Crash signal not sent to not-authorized session: '%s'",
                          session_bus_address);
                continue;
            }

            log_debug("Crash signal sent to authorized session: '%s'",
                      session_bus_address);

            GVariant *parameters = g_variant_new("(oi)",
                                                 obj->p2o_path,
                                                 (gint32)owner_uid);

            abrt_p2_object_emit_signal_with_destination(
                                                 service->pv->p2srv_p2_object,
                                                 "Crash",
                                                 parameters,
                                                 session_bus_address);
        }

        g_list_free(session_objects);
    }
}

struct entry_object_save_elements_context
{
    GDBusMethodInvocation *invocation;
    GVariant *elements;
};

static void entry_object_save_elements_cb(GObject *source_object,
            GAsyncResult *result,
            gpointer user_data)
{
    AbrtP2Entry *entry = ABRT_P2_ENTRY(source_object);
    struct entry_object_save_elements_context *context = user_data;

    g_variant_unref(context->elements);

    GError *error = NULL;
    GVariant *response = abrt_p2_entry_save_elements_finish(entry,
                                                            result,
                                                            &error);

    if (error == NULL)
    {
        g_dbus_method_invocation_return_value(context->invocation, response);
    }
    else
    {
        g_dbus_method_invocation_return_gerror(context->invocation, error);
        g_error_free(error);
    }

    g_object_unref(context->invocation);
    free(context);
}

struct entry_object_read_elements_context
{
    GDBusMethodInvocation *invocation;
    GVariant *elements;
    GUnixFDList *out_fd_list;
};

static void entry_object_read_elements_cb(GObject *source_object,
            GAsyncResult *result,
            gpointer user_data)
{
    AbrtP2Entry *entry = ABRT_P2_ENTRY(source_object);
    struct entry_object_read_elements_context *context = user_data;

    g_variant_unref(context->elements);

    GError *error = NULL;
    GVariant *response = abrt_p2_entry_read_elements_finish(entry, result, &error);
    if (error == NULL)
    {
        g_dbus_method_invocation_return_value_with_unix_fd_list(context->invocation,
                                                                response,
                                                                context->out_fd_list);
    }
    else
    {
        g_dbus_method_invocation_return_gerror(context->invocation, error);
        g_error_free(error);
    }

    g_object_unref(context->out_fd_list);
    g_object_unref(context->invocation);
    free(context);
}

static void entry_object_dbus_method_call(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant    *parameters,
            GDBusMethodInvocation *invocation,
            gpointer    user_data)
{
    log_debug("Problems2.Entry method : %s", method_name);

    AbrtP2Service *service = abrt_p2_object_service(user_data);

    GError *error = NULL;
    uid_t caller_uid = abrt_p2_service_caller_uid(service, caller, &error);
    if (caller_uid == (uid_t)-1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    GVariant *response = NULL;
    AbrtP2Entry *entry = abrt_p2_object_get_node(user_data);
    if (strcmp(method_name, "GetSemanticElement") == 0)
    {
        return;
    }
    else if (strcmp(method_name, "SetSemanticElement") == 0)
    {
        return;
    }
    else if (strcmp(method_name, "ReadElements") == 0)
    {
        struct entry_object_read_elements_context *context = xmalloc(sizeof(*context));
        context->invocation = g_object_ref(invocation);
        context->elements = g_variant_get_child_value(parameters, 0);
        context->out_fd_list = g_unix_fd_list_new();

        gint32 flags;
        g_variant_get_child(parameters, 1, "i", &flags);

        GCancellable *cancellable = g_cancellable_new();

        abrt_p2_entry_read_elements_async(entry,
                                          flags,
                                          context->elements,
                                          context->out_fd_list,
                                          caller_uid,
                                          service->pv->p2srv_max_message_size,
                                          service->pv->p2srv_max_message_unix_fds,
                                          cancellable,
                                          entry_object_read_elements_cb,
                                          context);

        return;
    }
    else if (strcmp(method_name, "SaveElements") == 0)
    {
        GDBusMessage *msg = g_dbus_method_invocation_get_message(invocation);
        GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(msg);

        struct entry_object_save_elements_context *context = xmalloc(sizeof(*context));

        context->invocation = g_object_ref(invocation);
        context->elements = g_variant_get_child_value(parameters, 0);

        gint32 flags;
        g_variant_get_child(parameters, 1, "i", &flags);

        ABRT_P2_ENTRY_SAVE_ELEMENTS_LIMITS_ON_STACK(limits,
                        abrt_p2_service_elements_limit(service, caller_uid),
                        abrt_p2_service_data_size_limit(service, caller_uid));

        GCancellable *cancellable = g_cancellable_new();

        abrt_p2_entry_save_elements_async(entry,
                                          flags,
                                          context->elements,
                                          fd_list,
                                          caller_uid,
                                          &limits,
                                          cancellable,
                                          entry_object_save_elements_cb,
                                          context);

        return;
    }
    else if (strcmp(method_name, "DeleteElements") == 0)
    {
        GVariant *elements = g_variant_get_child_value(parameters, 0);

        response = abrt_p2_entry_delete_elements(entry,
                                                 caller_uid,
                                                 elements,
                                                 &error);

        g_variant_unref(elements);
    }
    else
    {
        error_msg("BUG: org.freedesktop.Problems2.Entry does not have method: %s",
                  method_name);
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "The method has to be implemented");
    }

    if (error != NULL)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
    }
    else
        g_dbus_method_invocation_return_value(invocation, response);
}


#define GET_PLAIN_TEXT_PROPERTY(name, element) \
        if (strcmp(name, property_name) == 0) \
        { \
            char *tmp_value = dd_load_text_ext(dd, element, DD_FAIL_QUIETLY_ENOENT); \
            retval = g_variant_new_string(tmp_value ? tmp_value : ""); \
            free(tmp_value); \
            goto return_property_value; \
        }

#define GET_INTEGER_PROPERTY(name, element, S, def) \
        if (strcmp(name, property_name) == 0) \
        { \
            uint##S##_t tmp_value = def; \
            dd_load_uint##S (dd, element, &tmp_value); \
            retval = g_variant_new_uint##S ((guint##S)tmp_value); \
            goto return_property_value; \
        }

#define GET_UINT32_PROPERTY(name, element, def) GET_INTEGER_PROPERTY(name, element, 32, def)

static GVariant *entry_object_dbus_get_property(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GError      **error,
            gpointer    user_data)
{
    log_debug("Problems2.Entry get property : %s", property_name);

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t caller_uid = abrt_p2_service_caller_uid(service, caller, error);
    if (caller_uid == (uid_t)-1)
        return NULL;

    GVariant *retval;
    AbrtP2Entry *entry = abrt_p2_object_get_node(user_data);
    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(entry,
                                                      caller_uid,
                                                      DD_DONT_WAIT_FOR_LOCK | DD_OPEN_READONLY,
                                                      error);
    if (dd == NULL)
        return NULL;

    if (strcmp("ID", property_name) == 0)
    {
        retval = g_variant_new_string(dd->dd_dirname);
        goto return_property_value;
    }

    GET_PLAIN_TEXT_PROPERTY("User", FILENAME_USERNAME)
    GET_PLAIN_TEXT_PROPERTY("Hostname", FILENAME_HOSTNAME)
    GET_PLAIN_TEXT_PROPERTY("Type", FILENAME_TYPE)
    GET_PLAIN_TEXT_PROPERTY("Executable", FILENAME_EXECUTABLE)
    GET_PLAIN_TEXT_PROPERTY("CommandLineArguments", FILENAME_CMDLINE)
    GET_PLAIN_TEXT_PROPERTY("Component", FILENAME_COMPONENT)
    GET_PLAIN_TEXT_PROPERTY("UUID", FILENAME_UUID)
    GET_PLAIN_TEXT_PROPERTY("Duphash", FILENAME_DUPHASH)
    GET_PLAIN_TEXT_PROPERTY("Reason", FILENAME_REASON)
    GET_PLAIN_TEXT_PROPERTY("TechnicalDetails", FILENAME_NOT_REPORTABLE)

    GET_UINT32_PROPERTY("UID", FILENAME_UID, 0)
    GET_UINT32_PROPERTY("Count", FILENAME_COUNT, 1)

    if (strcmp("FirstOccurrence", property_name) == 0)
    {
        time_t tm = dd_get_first_occurrence(dd);
        if (tm == (time_t) -1)
        {
            dd_close(dd);
            g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Invalid problem data: FirstOccurrence cannot be returned");
            return NULL;
        }

        retval = g_variant_new_uint64((guint64)tm);
        goto return_property_value;
    }

    if (strcmp("LastOccurrence", property_name) == 0)
    {
        time_t ltm = dd_get_last_occurrence(dd);
        if (ltm == (time_t) -1)
        {
            dd_close(dd);
            g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Invalid problem data: LastOccurrence cannot be returned");
            return NULL;
        }

        retval = g_variant_new_uint64((guint64)ltm);
        goto return_property_value;
    }

    if (strcmp("Package", property_name) == 0)
    {
        const char *const elements[] = { FILENAME_PACKAGE,
                                         FILENAME_PKG_EPOCH,
                                         FILENAME_PKG_NAME,
                                         FILENAME_PKG_VERSION,
                                         FILENAME_PKG_RELEASE };

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("(sssss)"));
        for (size_t i = 0; i < ARRAY_SIZE(elements); ++i)
        {
            char *data = dd_load_text_ext(dd, elements[i], DD_FAIL_QUIETLY_ENOENT);
            g_variant_builder_add(&builder, "s", data);
            free(data);
        }

        retval = g_variant_builder_end(&builder);
        goto return_property_value;
    }

    if (strcmp("Reports", property_name) == 0)
    {
        GVariantBuilder top_builder;
        g_variant_builder_init(&top_builder, G_VARIANT_TYPE("a(sa{sv})"));

        GList *reports = read_entire_reported_to(dd);
        for (GList *iter = reports; iter != NULL; iter = g_list_next(iter))
        {
            GVariantBuilder value_builder;
            g_variant_builder_init(&value_builder, G_VARIANT_TYPE("a{sv}"));

            struct report_result *r = (struct report_result *)iter->data;

            if (r->url != NULL)
            {
                GVariant *data = g_variant_new_variant(g_variant_new_string(r->url));
                g_variant_builder_add(&value_builder, "{sv}", "URL", data);
            }
            if (r->msg != NULL)
            {
                GVariant *data = g_variant_new_variant(g_variant_new_string(r->msg));
                g_variant_builder_add(&value_builder, "{sv}", "MSG", data);
            }
            if (r->bthash != NULL)
            {
                GVariant *data = g_variant_new_variant(g_variant_new_string(r->bthash));
                g_variant_builder_add(&value_builder, "{sv}", "BTHASH", data);
            }

            GVariant *children[2];
            children[0] = g_variant_new_string(r->label);
            children[1] = g_variant_builder_end(&value_builder);
            GVariant *entry = g_variant_new_tuple(children, 2);

            g_variant_builder_add_value(&top_builder, entry);
        }

        g_list_free_full(reports, (GDestroyNotify)free_report_result);

        retval = g_variant_builder_end(&top_builder);

        goto return_property_value;
    }

    if (strcmp("Solutions", property_name) == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sssssi)"));

        /* TODO: not-yet-implemented - we don't know where to get the data */

        retval = g_variant_builder_end(&builder);
        goto return_property_value;
    }

    if (strcmp("Elements", property_name) == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        dd_init_next_file(dd);
        char *short_name;
        while (dd_get_next_file(dd, &short_name, NULL))
        {
            g_variant_builder_add(&builder, "s", short_name);
            free(short_name);
        }
        retval = g_variant_builder_end(&builder);
        goto return_property_value;
    }

    if (strcmp("SemanticElements", property_name) == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        /* no semantic elements yet */
        retval = g_variant_builder_end(&builder);
        goto return_property_value;
    }

    if (strcmp("IsReported", property_name) == 0)
    {
       retval = g_variant_new_boolean(dd_exist(dd, FILENAME_REPORTED_TO));
       goto return_property_value;
    }

    if (strcmp("CanBeReported", property_name) == 0)
    {
       retval = g_variant_new_boolean(!dd_exist(dd, FILENAME_NOT_REPORTABLE));
       goto return_property_value;
    }

    if (strcmp("IsRemote", property_name) == 0)
    {
       retval = g_variant_new_boolean(dd_exist(dd, FILENAME_REMOTE));
       goto return_property_value;
    }

    dd_close(dd);
    error_msg("Unknown property %s", property_name);
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
            "BUG: the property getter has to be implemented");
    return NULL;

return_property_value:
    dd_close(dd);
    return retval;
}

#ifdef PROBLEMS2_PROPERTY_SET
static gboolean entry_object_dbus_set_property(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GVariant    *args,
            GError      **error,
            gpointer    user_data)
{
    log_debug("Problems2.Entry set property : %s", property_name);

    uid_t caller_uid = abrt_p2_service_caller_uid(connection, caller, error);
    if (caller_uid == (uid_t)-1)
        return FALSE;

    AbrtP2Entry *entry = abrt_p2_object_get_node(user_data);
    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(entry,
                                                      caller_uid,
                                                      DD_DONT_WAIT_FOR_LOCK,
                                                      error);
    if (entry == NULL)
        return FALSE;

    if (strcmp("id", property_name) == 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_PROPERTY_READ_ONLY);
        return FALSE;
    }

    if (strcmp("uid", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("user", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("hostname", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("type", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("first_occurrence", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("last_occurrence", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("count", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("executable", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("command_line_arguments", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("component", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("package", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("uuid", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("duphash", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("reports", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("reason", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("solutions", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("technical_details", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("elements", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("semantic_elements", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("is_reported", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("can_be_reported", property_name) == 0)
    {
        return FALSE;
    }

    if (strcmp("is_remote", property_name) == 0)
    {
        return FALSE;
    }

    error_msg("Unknown property %s", property_name);
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                "BUG: the property setter has to be implemented");
    return FALSE;
}
#endif/*PROBLEMS2_PROPERTY_SET*/

static void entry_object_destructor(AbrtP2Object *obj)
{
    AbrtP2Entry *entry = (AbrtP2Entry *)obj->node;
    g_object_unref(entry);
}

static char *entry_object_dir_name_to_path(const char *dd_dirname)
{
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    str_to_sha1str(hash_str, dd_dirname);
    return xasprintf(ABRT_P2_PATH"/Entry/%s", hash_str);
}

static AbrtP2Object *entry_object_register_dump_dir(AbrtP2Service *service,
                const char *dd_dirname,
                GError **error)
{
    char *const dup_dirname = xstrdup(dd_dirname);
    AbrtP2Entry *entry = abrt_p2_entry_new(dup_dirname);

    return abrt_p2_service_register_entry(service, entry, error);
}

AbrtP2Object *abrt_p2_service_register_entry(AbrtP2Service *service,
            struct _AbrtP2Entry *entry,
            GError **error)
{
    const char *dd_dirname = abrt_p2_entry_problem_id(entry);
    log_debug("Registering problem entry for directory: %s", dd_dirname);
    char *path = entry_object_dir_name_to_path(dd_dirname);

    AbrtP2Object *obj = abrt_p2_object_new(service,
                                           &(service->pv->p2srv_p2_entry_type),
                                           path,
                                           entry,
                                           entry_object_destructor,
                                           error);

    if (obj == NULL)
    {
        g_prefix_error(error, "Failed to register Entry object for directory '%s': ",
                       dd_dirname);
        return NULL;
    }

    struct dump_dir *dd = dd_opendir(dd_dirname, DD_OPEN_FD_ONLY);
    uid_t owner = dd_get_owner(dd);
    dd_close(dd);

    struct user_info *user = abrt_p2_service_user_lookup(service, owner);

    if (user == NULL)
        user = abrt_p2_service_user_new(service, owner);

    if (user->problems == ULONG_MAX)
    {
        /* Give up, we cannot recover from this. */
        error_msg_and_die("Too many problems owned by a single user: uid=%lu",
                          (long unsigned)owner);
    }

    user->problems++;

    return obj;
}

struct entry_object_save_problem_args
{
    AbrtP2EntrySaveElementsLimits limits;
    GVariant *problem_info;
    GUnixFDList *fd_list;
    uid_t caller_uid;
    GError **error;
};

static int entry_object_wrapped_abrt_p2_entry_save_elements(struct dump_dir *dd,
            struct entry_object_save_problem_args *args)
{
    return abrt_p2_entry_save_elements_in_dump_dir(dd,
                                                   ABRT_P2_ENTRY_ALL_FATAL,
                                                   args->problem_info,
                                                   args->fd_list,
                                                   args->caller_uid,
                                                   &(args->limits),
                                                   args->error);
}

static GList *abrt_g_variant_get_dict_keys(GVariant *dict)
{
    gchar *name = NULL;
    GVariant *value = NULL;
    GVariantIter iter;
    g_variant_iter_init(&iter, dict);

    GList *retval = NULL;
    /* No need to free 'name' and 'container' unless breaking out of the loop */
    while (g_variant_iter_loop(&iter, "{sv}", &name, &value))
        retval = g_list_prepend(retval, xstrdup(name));

    return retval;
}

char *abrt_p2_service_save_problem( AbrtP2Service *service,
            GVariant *problem_info,
            GUnixFDList *fd_list,
            uid_t caller_uid,
            GError **error)
{
    GVariantDict pd;
    g_variant_dict_init(&pd, problem_info);

    /* Re-implement problem_data_add_basics(problem_info); - I don't want to
     * convert GVariant* to problem_data_t and back.
     *
     * The problem data should be converted to some kind of interface!
     */
    char *analyzer_str = NULL;
    GVariant *analyzer_element = g_variant_dict_lookup_value(&pd,
                                                             FILENAME_ANALYZER,
                                                             G_VARIANT_TYPE_STRING);
    if (analyzer_element == NULL)
    {
        analyzer_str = xstrdup("libreport");
        g_variant_dict_insert(&pd, FILENAME_ANALYZER, "s", analyzer_str);
    }
    else
    {
        analyzer_str = xstrdup(g_variant_get_string(analyzer_element, NULL));
        g_variant_unref(analyzer_element);
    }

    char *type_str = NULL;
    GVariant *type_element = g_variant_dict_lookup_value(&pd,
                                                         FILENAME_TYPE,
                                                         G_VARIANT_TYPE_STRING);
    if (type_element == NULL)
    {
         type_str = xstrdup(analyzer_str);
    }
    else
    {
         type_str = xstrdup(g_variant_get_string(type_element, NULL));
         g_variant_unref(type_element);
    }

    GVariant *uuid_element = g_variant_dict_lookup_value(&pd,
                                                         FILENAME_UUID,
                                                         G_VARIANT_TYPE_STRING);
    if (uuid_element != NULL)
    {
        g_variant_unref(uuid_element);
    }
    else
    {
        GVariant *duphash_element = g_variant_dict_lookup_value(&pd,
                                                                FILENAME_DUPHASH,
                                                                G_VARIANT_TYPE_STRING);
        if (duphash_element != NULL)
        {
            g_variant_dict_insert_value(&pd, FILENAME_UUID, duphash_element);
            g_variant_unref(duphash_element);
        }
        else
        {
            /* start hash */
            sha1_ctx_t sha1ctx;
            sha1_begin(&sha1ctx);

            /*
             * To avoid spurious hash differences, sort keys so that elements are
             * always processed in the same order:
             */
            GList *list = abrt_g_variant_get_dict_keys(problem_info);
            list = g_list_sort(list, (GCompareFunc)strcmp);
            for (GList *l = list; l != NULL; l = g_list_next(l))
            {
                GVariant *element = g_variant_dict_lookup_value(&pd,
                                                                (const char *)l->data,
                                                                G_VARIANT_TYPE_STRING);
                /* do not hash items which are binary or file descriptor */
                if (element == NULL)
                    continue;

                gsize size = 0;
                const char *content = g_variant_get_string(element, &size);
                sha1_hash(&sha1ctx, content, size);

                g_variant_unref(element);
            }
            g_list_free_full(list, free);

            /* end hash */
            char hash_bytes[SHA1_RESULT_LEN];
            sha1_end(&sha1ctx, hash_bytes);
            char hash_str[SHA1_RESULT_LEN*2 + 1];
            bin2hex(hash_str, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';

            g_variant_dict_insert(&pd, FILENAME_UUID, "s", hash_str);
        }
    }

    /* Sanitize UID
     */
    GVariant *uid_element =  g_variant_dict_lookup_value(&pd,
                                                         FILENAME_UID,
                                                         G_VARIANT_TYPE_STRING);
    if (caller_uid != 0 || uid_element == NULL)
    {   /* set uid field to caller's uid
           if caller is not root or root doesn't pass own uid */
        log_info("Adding UID %lu to the problem info", (long unsigned)caller_uid);
        char *uid_str = xasprintf("%lu", (long unsigned)caller_uid);
        g_variant_dict_insert(&pd, FILENAME_UID, "s", uid_str);
        free(uid_str);
    }

    if (uid_element != NULL)
        g_variant_unref(uid_element);

    struct entry_object_save_problem_args args = {
        .problem_info = g_variant_dict_end(&pd),
        .fd_list = fd_list,
        .caller_uid = caller_uid,
        .error = error,
    };

    ABRT_P2_ENTRY_SAVE_ELEMENTS_LIMITS_INITIALIZER(args.limits,
                        abrt_p2_service_elements_limit(service, caller_uid),
                        abrt_p2_service_data_size_limit(service, caller_uid));

    struct dump_dir *dd = create_dump_dir(g_settings_dump_location,
                                          type_str,
                                          /*fs owner*/0,
                                          (save_data_call_back)entry_object_wrapped_abrt_p2_entry_save_elements,
                                          (void *)&args);

    g_variant_unref(args.problem_info);
    free(type_str);
    free(analyzer_str);

    if (dd == NULL)
    {
        g_prefix_error(error, "Failed to create new problem directory: ");
        return NULL;
    }

    char *retval = xstrdup(dd->dd_dirname);
    dd_close(dd);

    return retval;
}

static
AbrtP2Object *abrt_p2_service_get_entry_object(AbrtP2Service *service,
            const char *entry_path,
            int flags,
            GError **error)
{
    AbrtP2Object *obj = problems2_object_type_get_object(&(service->pv->p2srv_p2_entry_type),
                                                         entry_path);

    if (obj == NULL && !(flags & ABRT_P2_SERVICE_ENTRY_LOOKUP_OPTIONAL))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_BAD_ADDRESS,
                "Requested Entry does not exist");
    }

    return obj;
}

AbrtP2Object *abrt_p2_service_get_entry_for_problem(AbrtP2Service *service,
            const char *problem_id,
            int flags,
            GError **error)
{
    char *entry_path = entry_object_dir_name_to_path(problem_id);
    AbrtP2Object *obj = abrt_p2_service_get_entry_object(service,
                                                         entry_path,
                                                         flags,
                                                         error);
    free(entry_path);

    return obj;
}

int abrt_p2_service_remove_problem(AbrtP2Service *service,
            const char *entry_path,
            uid_t caller_uid,
            GError **error)
{
    AbrtP2Object *obj = abrt_p2_service_get_entry_object(service,
                                                         entry_path,
                                                         ABRT_P2_SERVICE_ENTRY_LOOKUP_NOFLAGS,
                                                         error);
    if (obj == NULL)
    {
        log_debug("The requested Problem Entry does not exist");
        return -ENOENT;
    }

    AbrtP2Entry *entry = ABRT_P2_ENTRY(abrt_p2_object_get_node(obj));
    if (abrt_p2_entry_state(entry) != ABRT_P2_ENTRY_STATE_COMPLETE)
    {
        log_debug("Cannot remove temporary/deleted Problem Entry");
        return -EINVAL;
    }

    const int ret = abrt_p2_entry_delete(entry, caller_uid, error);
    if (ret != 0)
    {
        log_debug("Failed to remove Entry's data directory");
        return ret;
    }

    abrt_p2_object_destroy(obj);
    return 0;
}

GVariant *abrt_p2_service_entry_problem_data(AbrtP2Service *service,
            const char *entry_path,
            uid_t caller_uid,
            GError **error)
{
    AbrtP2Object *obj = abrt_p2_service_get_entry_object(service,
                                                         entry_path,
                                                         ABRT_P2_SERVICE_ENTRY_LOOKUP_NOFLAGS,
                                                         error);
    if (obj == NULL)
        return NULL;

    return abrt_p2_entry_problem_data(ABRT_P2_ENTRY(obj->node), caller_uid, error);
}

/*
 * /org/freedesktop/Problems2/Task
 */
static void task_object_on_status_changed(AbrtP2Task *task,
            gint32 status,
            gpointer user_data);

static void task_object_on_canceled_task(AbrtP2Task *task,
            gint32 status,
            gpointer user_data);

static void task_object_dispose(AbrtP2Object *obj)
{
    AbrtP2Task *task = abrt_p2_object_get_node(obj);

    gulong sig_handler = g_signal_handler_find(task,
                                               G_SIGNAL_MATCH_FUNC,
                                               0,
                                               0,
                                               NULL,
                                               task_object_on_status_changed,
                                               NULL);

    if (sig_handler != 0)
        g_signal_handler_disconnect(task, sig_handler);

    sig_handler = g_signal_handler_find(task,
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        NULL,
                                        task_object_on_canceled_task,
                                        NULL);

    if (sig_handler != 0)
        g_signal_handler_disconnect(task, sig_handler);

    abrt_p2_object_destroy(obj);
}

struct task_cancel_args
{
    AbrtP2Object  *tca_object;
    AbrtP2Session *tca_session;
};

static void task_object_on_canceled_task(AbrtP2Task *task,
            gint32 status,
            gpointer user_data)
{
    struct task_cancel_args *args = (struct task_cancel_args *)user_data;

    if (status != ABRT_P2_TASK_STATUS_CANCELED)
        log_warning("Canceled task moved to another state than CANCELLED");

    GError *local_error = NULL;
    abrt_p2_session_remove_task(args->tca_session, task, &local_error);
    g_object_unref(args->tca_session);

    if (local_error != NULL)
    {
        error_msg("BUG: failed to remove task from session: %s",
                  local_error->message);
        g_error_free(local_error);
    }

    log_debug("Disposing canceled task");
    task_object_dispose(args->tca_object);

    free(args);

    g_object_unref(task);
}

static void task_object_on_status_changed(AbrtP2Task *task,
            gint32 status,
            gpointer user_data)
{
    AbrtP2Object *object = (AbrtP2Object *)user_data;
    GDBusMessage *message = g_dbus_message_new_signal(object->p2o_path,
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged");

    g_dbus_message_set_sender(message, ABRT_P2_BUS);

    GVariantDict properties_changed;
    g_variant_dict_init(&properties_changed, NULL);
    g_variant_dict_insert(&properties_changed, "status", "v", g_variant_new_int32(status));

    GVariant *children[3];
    children[0] = g_variant_new_string(object->p2o_type->iface->name);
    children[1] = g_variant_dict_end(&properties_changed);
    children[2] = g_variant_new("as", NULL);

    GVariant *parameters = g_variant_new_tuple(children, 3);

    g_dbus_message_set_body(message, parameters);

    if (g_verbose > 2)
    {
        gchar *pstr = g_variant_print(parameters, TRUE);
        log_debug("Emitting signal '%s' : (%s)",
                  "PropertiesChanged",
                  pstr);
        g_free(pstr);
    }

    GError *error = NULL;
    g_dbus_connection_send_message(abrt_p2_service_dbus(object->p2o_service),
                                   message,
                                   G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                   NULL,
                                   &error);
    g_object_unref(message);
    if (error != NULL)
    {
        error_msg("Failed to emit signal '%s': %s",
                  "PropertiesChanged",
                  error->message);
        g_free(error);
    }
}

static AbrtP2Object *task_object_register(AbrtP2Service* service,
            AbrtP2Object *session_obj,
            AbrtP2Task *task,
            GError **error)
{
    AbrtP2Session *session = abrt_p2_object_get_node(session_obj);
    uint32_t regid = abrt_p2_session_add_task(session, task, error);
    if (*error != NULL)
        return NULL;

    const char *session_path = abrt_p2_object_path(session_obj);
    char *path = xasprintf("%s/Task/%u", session_path, regid);

    AbrtP2Object *obj = abrt_p2_object_new(service,
                                           &(service->pv->p2srv_p2_task_type),
                                           path,
                                           task,
                                           NULL,
                                           error);

    if (obj == NULL)
    {
        g_prefix_error(error, "Failed to register Task object: ");
        return NULL;
    }

    g_signal_connect(task,
                     "status-changed",
                     G_CALLBACK(task_object_on_status_changed),
                     obj);

    return obj;
}

static void task_object_dbus_method_call(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant    *parameters,
            GDBusMethodInvocation *invocation,
            gpointer    user_data)
{
    log_debug("Problems2.Task method : %s", method_name);

    /* Check sanity */
    if (strcmp(interface_name, ABRT_P2_NS_MEMBER("Task")) != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return;
    }

    GError *error = NULL;

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t real_caller_uid = abrt_p2_service_caller_real_uid(service, caller, &error);
    if (real_caller_uid == (uid_t) -1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    AbrtP2Task *task = abrt_p2_object_get_node(user_data);
    AbrtP2Object *session_obj = abrt_p2_service_get_session_for_caller(service,
                                                                       caller,
                                                                       real_caller_uid,
                                                                       &error);
    if (error != NULL)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    AbrtP2Session *session = abrt_p2_object_get_node(session_obj);
    if (abrt_p2_session_owns_task(session, task) != 0)
    {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_ACCESS_DENIED,
                                              "The task does not belong to your session");
        return;
    }

    GVariant *response = NULL;

    if (strcmp("Start", method_name) == 0)
    {
        GVariant *options = g_variant_get_child_value(parameters, 0);
        abrt_p2_task_start(task, options, &error);
        g_variant_unref(options);
    }
    else if (strcmp("Cancel", method_name) == 0)
    {
        struct task_cancel_args *tca = xmalloc(sizeof(*tca));
        tca->tca_object = user_data;
        tca->tca_session = g_object_ref(session);

        int s = g_signal_connect(task,
                                 "status-changed",
                                 G_CALLBACK(task_object_on_canceled_task),
                                 tca);

        abrt_p2_task_cancel(task, &error);

        if (error != NULL)
        {
            g_signal_handler_disconnect(task, s);
            free(tca);
        }
    }
    else if (strcmp("Finish", method_name) == 0)
    {
        GVariant *results = NULL;
        gint32 code = -1;
        abrt_p2_task_finish(task, &results, &code, &error);

        if (error == NULL)
        {
            GVariant *children[2];
            children[0] = results;
            children[1] = g_variant_new_int32(code);

            response = g_variant_new_tuple(children, 2);

            GError *local_error = NULL;
            abrt_p2_session_remove_task(session, task, &local_error);
            if (local_error != NULL)
            {
                error_msg("BUG: failed to remove task from session: %s",
                          local_error->message);
                g_error_free(local_error);
            }

            task_object_dispose(user_data);

            g_object_unref(task);
        }
    }
    else
    {
        error_msg("BUG: org.freedesktop.Problems2.Task does not have method: %s",
                  method_name);

        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "org.freedesktop.Problems2.Task is missing the implementation of the method");
        return;
    }

    if (error)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
    }
    else
        g_dbus_method_invocation_return_value(invocation, response);
}

static GVariant *task_object_dbus_get_property(GDBusConnection *connection,
            const gchar *caller,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GError      **error,
            gpointer    user_data)
{
    log_debug("Problems2.Task get property : %s", property_name);

    if (strcmp(interface_name, ABRT_P2_NS_MEMBER("Task")) != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return NULL;
    }

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t real_caller_uid = abrt_p2_service_caller_real_uid(service,
                                                            caller,
                                                            error);
    if (real_caller_uid == (uid_t) -1)
    {
        error_msg("Failed to get uid of the caller");
        return NULL;
    }

    AbrtP2Task *task = abrt_p2_object_get_node(user_data);
    AbrtP2Object *session_obj = abrt_p2_service_get_session_for_caller(service,
                                                                       caller,
                                                                       real_caller_uid,
                                                                       error);
    if (*error != NULL)
    {
        error_msg("Failed to get session for the caller");
        return NULL;
    }

    AbrtP2Session *session = abrt_p2_object_get_node(session_obj);
    if (abrt_p2_session_owns_task(session, task) != 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "The task does not belong to your session");
        return NULL;
    }

    if (strcmp("Details", property_name) == 0)
    {
        return abrt_p2_task_details(task);
    }

    if (strcmp("Status", property_name) == 0)
    {
        return g_variant_new_int32(abrt_p2_task_status(task));
    }

    error_msg("BUG: org.freedesktop.Problems2.Task does not have property: %s", property_name);

    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                "org.freedesktop.Problems2.Task is missing the implementation of the property");

    return NULL;
}


/*
 * /org/freedesktop/Problems2
 */
GVariant *abrt_p2_service_new_problem(AbrtP2Service *service,
                AbrtP2Object *session_obj,
                GVariant *problem_info,
                gint32 flags,
                uid_t caller_uid,
                GUnixFDList *fd_list,
                GError **error)
{
    AbrtP2TaskNewProblem *p2t_np = abrt_p2_task_new_problem_new(service,
                                                                problem_info,
                                                                caller_uid,
                                                                fd_list ? g_object_ref(fd_list) : NULL);

    log_debug("Created task '%p' for session '%s'", p2t_np, abrt_p2_object_path(session_obj));
    if (!(flags & 0x1))
    {
        log_debug("Running NewProblem task '%p' in autonomous mode", p2t_np);
        abrt_p2_task_autonomous_run(ABRT_P2_TASK(p2t_np), error);
        return g_variant_new("(o)", "/");
    }

    if (flags & 0x2)
    {
        log_debug("Configuring NewProblem task '%p' to stop after creating a temporary directory", p2t_np);
        abrt_p2_task_new_problem_wait_before_notify(p2t_np, true);
    }

    AbrtP2Object *obj = task_object_register(service, session_obj, ABRT_P2_TASK(p2t_np), error);
    if (obj == NULL)
    {
        g_object_unref(p2t_np);
        g_prefix_error(error, "Cannot export NewProblem task on D-Bus: ");
        return NULL;
    }

    if (flags & 0x4)
    {
        log_debug("NewProblem task '%p' will be automatically started", p2t_np);
        abrt_p2_task_start(ABRT_P2_TASK(p2t_np), NULL, error);
    }

    return g_variant_new("(o)", obj->p2o_path);
}

/**
 * Converts caller to the path of session object
 */
GVariant *abrt_p2_service_callers_session(AbrtP2Service *service,
                const char *caller,
                GError **error)
{
    const char *session_path = abrt_p2_service_session_path(service, caller, error);

    if (session_path == NULL)
        return NULL;

    return g_variant_new("(o)", session_path);
}

GVariant *abrt_p2_service_get_problems(AbrtP2Service *service,
                uid_t caller_uid,
                gint32 flags,
                GVariant *options,
                GError **error)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, service->pv->p2srv_p2_entry_type.objects);

    log_debug("Going through entries");
    const char *entry_path;
    AbrtP2Object *entry_obj;
    while(g_hash_table_iter_next(&iter, (gpointer)&entry_path, (gpointer)&entry_obj))
    {
        bool singleout = flags == 0;

        AbrtP2Entry *entry = abrt_p2_object_get_node(entry_obj);
        int state = abrt_p2_entry_state(entry);

        log_debug("Entry: %s", entry_path);
        if (state == ABRT_P2_ENTRY_STATE_DELETED)
        {
            continue;
        }
        else if (state == ABRT_P2_ENTRY_STATE_NEW)
        {
            if (flags == 0)
                continue;

            singleout = singleout || (flags & ABRT_P2_SERVICE_GET_PROBLEM_FLAGS_NEW);
        }

        if (0 != abrt_p2_entry_accessible_by_uid(entry, caller_uid, NULL))
        {
            if (flags == 0)
                continue;

            log_debug("Entry not accessible: %s", entry_path);
            singleout = singleout || (flags & ABRT_P2_SERVICE_GET_PROBLEM_FLAGS_FOREIGN);
        }

        if (singleout)
        {
            log_debug("Adding entry: %s", entry_path);
            g_variant_builder_add(&builder, "o", entry_path);
        }
    }


    GVariant *retval_body[1];
    retval_body[0] = g_variant_builder_end(&builder);
    return  g_variant_new_tuple(retval_body, ARRAY_SIZE(retval_body));
}


GVariant *abrt_p2_service_delete_problems(AbrtP2Service *service,
                GVariant *entries,
                uid_t caller_uid,
                GError **error)
{
    GVariantIter *iter;
    gchar *entry_path;
    g_variant_get(entries, "ao", &iter);
    while (g_variant_iter_loop(iter, "o", &entry_path))
    {
        log_debug("Removing Problem Entry: '%s'", entry_path);
        const int r = abrt_p2_service_remove_problem(service,
                                                     entry_path,
                                                     caller_uid,
                                                     error);

        if (r != 0)
        {
            g_free(entry_path);
            break;
        }
    }

    g_variant_iter_free(iter);
    return NULL;
}

/* D-Bus method handler
 */
static void p2_object_dbus_method_call(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant    *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer    user_data)
{
    log_debug("Problems2 method : %s", method_name);

    /* Check sanity */
    if (strcmp(interface_name, "org.freedesktop.Problems2") != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return;
    }

    GVariant *response = NULL;
    GError *error = NULL;

    AbrtP2Service *service = abrt_p2_object_service(user_data);
    uid_t caller_uid = abrt_p2_service_caller_uid(service, caller, &error);
    if (caller_uid == (uid_t) -1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    if (strcmp("NewProblem", method_name) == 0)
    {
        AbrtP2Object *session_obj = abrt_p2_service_get_session_for_caller(service,
                                                                           caller,
                                                                           caller_uid,
                                                                           &error);
        if (session_obj != NULL)
        {
            GDBusMessage *msg = g_dbus_method_invocation_get_message(invocation);
            GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(msg);

            GVariant *data = g_variant_get_child_value(parameters, 0);
            gint32 flags;
            g_variant_get_child(parameters, 1, "i", &flags);

            response = abrt_p2_service_new_problem(service,
                                                   session_obj,
                                                   data,
                                                   flags,
                                                   caller_uid,
                                                   fd_list,
                                                   &error);
        }
    }
    else if (strcmp("GetSession", method_name) == 0)
    {
        response = abrt_p2_service_callers_session(service, caller, &error);
    }
    else if (strcmp("GetProblems", method_name) == 0)
    {
        GVariant *flags_param = g_variant_get_child_value(parameters, 0);
        GVariant *options_param = g_variant_get_child_value(parameters, 1);

        response = abrt_p2_service_get_problems(service,
                                                caller_uid,
                                                g_variant_get_int32(flags_param),
                                                options_param,
                                                &error);

        g_variant_unref(options_param);
        g_variant_unref(flags_param);
    }
    else if (strcmp("GetProblemData", method_name) == 0)
    {
        /* Parameter tuple is (0) */
        const char *entry_path;
        g_variant_get(parameters, "(&o)", &entry_path);

        response = abrt_p2_service_entry_problem_data(service,
                                                      entry_path,
                                                      caller_uid,
                                                      &error);
    }
    else if (strcmp("DeleteProblems", method_name) == 0)
    {
        GVariant *array = g_variant_get_child_value(parameters, 0);
        response = abrt_p2_service_delete_problems(service, array, caller_uid, &error);
        g_variant_unref(array);
    }
    else
    {
        error_msg("BUG: org.freedesktop.Problems2 does not have method: %s",
                  method_name);
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "The method has to be implemented");
        return;
    }

    if (error != NULL)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    g_dbus_method_invocation_return_value(invocation, response);
    return;
}

/*
 * Service functions
 */
static void abrt_p2_service_private_destroy(AbrtP2ServicePrivate *pv)
{
    if (pv->p2srv_connected_users != NULL)
    {
        g_hash_table_destroy(pv->p2srv_connected_users);
        pv->p2srv_connected_users = NULL;
    }

    problems2_object_type_destroy(&(pv->p2srv_p2_type));
    problems2_object_type_destroy(&(pv->p2srv_p2_session_type));
    problems2_object_type_destroy(&(pv->p2srv_p2_entry_type));
    problems2_object_type_destroy(&(pv->p2srv_p2_task_type));

    if (pv->p2srv_proxy_dbus != NULL)
    {
        g_object_unref(pv->p2srv_proxy_dbus);
        pv->p2srv_proxy_dbus = NULL;
    }

    if (pv->p2srv_pk_authority != NULL)
    {
        pv->p2srv_pk_authority = NULL;
        --g_polkit_authority_refs;

        if (g_polkit_authority_refs == 0)
        {
            PolkitAuthority *pk = abrt_p2_session_class_release_polkit_authority();
            if (pk != g_polkit_authority)
                log_notice("Session class uses custom Polkit Authority");
            else
            {
                g_object_unref(g_polkit_authority);
                g_polkit_authority = NULL;
            }
        }
    }
}

static int abrt_p2_service_private_init(AbrtP2ServicePrivate *pv,
            GError **unused)
{
    pv->p2srv_max_message_size = DBUS_MAXIMUM_MESSAGE_LENGTH;
    pv->p2srv_max_message_unix_fds = 16;

    pv->p2srv_limit_clients = 5;
    pv->p2srv_limit_elements = 100;
    pv->p2srv_limit_data_size = 2L*1024L*1024L*1023L;
    pv->p2srv_limit_user_problems = 1000;
    pv->p2srv_limit_new_problem_throttling_magnitude = 4;
    pv->p2srv_limit_new_problems_batch = 10;

    int r = 0;
    {
        static GDBusInterfaceVTable p2_object_vtable = {
            .method_call = p2_object_dbus_method_call,
            .get_property = NULL,
            .set_property = NULL,
        };

        r = problems2_object_type_init(&(pv->p2srv_p2_type),
                                       g_org_freedesktop_Problems2_xml,
                                       &p2_object_vtable);
        if (r != 0)
        {
            log_notice("Failed to initialize org.freedesktop.Problems2 type");
            goto error_return;
        }
    }

    {
        static GDBusInterfaceVTable session_object_vtable = {
            .method_call = session_object_dbus_method_call,
            .get_property = session_object_dbus_get_property,
            .set_property = NULL,
        };

        r = problems2_object_type_init(&(pv->p2srv_p2_session_type),
                                       g_org_freedesktop_Problems2_Session_xml,
                                       &session_object_vtable);
        if (r != 0)
        {
            log_notice("Failed to initialize org.freedesktop.Problems2.Session type");
            goto error_return;
        }
    }

    {
        static GDBusInterfaceVTable entry_object_vtable = {
            .method_call = entry_object_dbus_method_call,
            .get_property = entry_object_dbus_get_property,
            .set_property = NULL,
        };

        r = problems2_object_type_init(&(pv->p2srv_p2_entry_type),
                                       g_org_freedesktop_Problems2_Entry_xml,
                                       &entry_object_vtable);
        if (r != 0)
        {
            log_notice("Failed to initialize org.freedesktop.Problems2.Entry type");
            goto error_return;
        }
    }

    {
        static GDBusInterfaceVTable task_object_vtable = {
            .method_call = task_object_dbus_method_call,
            .get_property = task_object_dbus_get_property,
            .set_property = NULL,
        };

        r = problems2_object_type_init(&(pv->p2srv_p2_task_type),
                                       g_org_freedesktop_Problems2_Task_xml,
                                       &task_object_vtable);
        if (r != 0)
        {
            log_notice("Failed to initialize org.freedesktop.Problems2.Task type");
            goto error_return;
        }
    }

    pv->p2srv_connected_users = g_hash_table_new_full(g_direct_hash,
                                                      g_direct_equal,
                                                      NULL,
                                                      (GDestroyNotify)user_info_free);

    if (g_polkit_authority != NULL)
    {
        ++g_polkit_authority_refs;
        pv->p2srv_pk_authority = g_polkit_authority;
        return 0;
    }

    GError *local_error = NULL;
    g_polkit_authority = pv->p2srv_pk_authority = polkit_authority_get_sync(NULL,
                                                                            &local_error);
    if (pv->p2srv_pk_authority == NULL)
    {
        r = -1;
        log_notice("Failed to get PolkitAuthority: %s", local_error->message);
        g_error_free(local_error);
        goto error_return;
    }

    ++g_polkit_authority_refs;
    abrt_p2_session_class_set_polkit_authority(g_polkit_authority);
    return 0;

error_return:
    abrt_p2_service_private_destroy(pv);
    return r;
}

static void abrt_p2_service_finalize(GObject *gobject)
{
    AbrtP2ServicePrivate *pv = abrt_p2_service_get_instance_private(ABRT_P2_SERVICE(gobject));
    abrt_p2_service_private_destroy(pv);
}

static void abrt_p2_service_class_init(AbrtP2ServiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = abrt_p2_service_finalize;

    service_signals[SERVICE_SIGNALS_NEW_CLIENT_CONNECTED] =
        g_signal_newv("new-client-connected",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                     NULL /* closure */,
                     NULL /* accumulator */,
                     NULL /* accumulator data */,
                     NULL /* C marshaller */,
                     G_TYPE_NONE /* return_type */,
                     0     /* n_params */,
                     NULL  /* param_types */);

    service_signals[SERVICE_SIGNALS_ALL_CLIENTS_DISCONNECTED] =
        g_signal_newv("all-clients-disconnected",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                     NULL /* closure */,
                     NULL /* accumulator */,
                     NULL /* accumulator data */,
                     NULL /* C marshaller */,
                     G_TYPE_NONE /* return_type */,
                     0     /* n_params */,
                     NULL  /* param_types */);
}

static void abrt_p2_service_init(AbrtP2Service *self)
{
    self->pv = abrt_p2_service_get_instance_private(self);
}

AbrtP2Service *abrt_p2_service_new(GError **error)
{
    AbrtP2Service *service = g_object_new(TYPE_ABRT_P2_SERVICE, NULL);

    if (abrt_p2_service_private_init(service->pv, error) != 0)
    {
        g_object_unref(service);
        return NULL;
    }

    return service;
}

static struct user_info *abrt_p2_service_user_lookup(AbrtP2Service *service,
            uid_t uid)
{
    return  g_hash_table_lookup(service->pv->p2srv_connected_users,
                                GUINT_TO_POINTER((guint)uid));
}

static struct user_info *abrt_p2_service_user_insert(AbrtP2Service *service,
            uid_t uid,
            struct user_info *user)
{
    g_hash_table_insert(service->pv->p2srv_connected_users,
                        GUINT_TO_POINTER((guint)uid),
                        user);
    return user;
}

static struct user_info *abrt_p2_service_user_new(AbrtP2Service *service,
            uid_t uid)
{
    struct user_info *user = user_info_new();
    return abrt_p2_service_user_insert(service, uid, user);
}

static GDBusConnection *abrt_p2_service_dbus(AbrtP2Service *service)
{
    return service->pv->p2srv_dbus;
}

struct bridge_call_args
{
    AbrtP2Service *service;
    GError **error;
};

static int bridge_register_dump_dir_entry_node(struct dump_dir *dd,
            void *call_args)
{
    struct bridge_call_args *args = call_args;
    return NULL == entry_object_register_dump_dir(args->service,
                                                  dd->dd_dirname,
                                                  args->error);
}

static void on_g_signal(GDBusProxy *proxy,
            gchar      *sender_name,
            gchar      *signal_name,
            GVariant   *parameters,
            gpointer    user_data)
{
    if (0 != strcmp(signal_name, "NameOwnerChanged"))
        return;

    const gchar *bus_name = NULL;
    const gchar *old_owner = NULL;
    const gchar *new_owner = NULL;

    g_variant_get(parameters, "(&s&s&s)", &bus_name, &old_owner, &new_owner);

    if (bus_name[0] == '\0' || old_owner[0] == '\0' || new_owner[0] != '\0')
        return;

    AbrtP2Service *service = ABRT_P2_SERVICE(user_data);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, service->pv->p2srv_p2_session_type.objects);

    const char *session_path;
    AbrtP2Object *session_obj;
    while(g_hash_table_iter_next(&iter, (gpointer)&session_path, (gpointer)&session_obj))
    {
        AbrtP2Session *session = abrt_p2_object_get_node(session_obj);
        const char *session_caller = abrt_p2_session_caller(session);
        if (strcmp(bus_name, session_caller) != 0)
            continue;

        log_debug("Bus '%s' disconnected: destroying session: %s", bus_name, session_path);

        GHashTableIter task_iter;
        g_hash_table_iter_init(&task_iter, service->pv->p2srv_p2_task_type.objects);

        const char *task_path;
        AbrtP2Object *task_obj;
        const size_t session_path_len = strlen(session_path);
        while(g_hash_table_iter_next(&task_iter, (gpointer)&task_path, (gpointer)&task_obj))
        {
            if (strncmp(task_path, session_path, session_path_len) != 0)
                continue;

            if (strncmp(task_path + session_path_len, "/Task/", strlen("/Task/")) != 0)
                continue;

            log_debug("Destroying Task of disconnected session: %s", task_path);

            /* Not touching the linked task, destroying of the session object
             * below should call abrt_p2_session_clean_tasks() which will unref
             * all session tasks.
             */
            task_object_dispose(task_obj);
        }

        /* session_path belongs to session_obj */
        abrt_p2_object_destroy(session_obj);
    }
}

int abrt_p2_service_register_objects(AbrtP2Service *service, GDBusConnection *connection, GError **error)
{
    if (service->pv->p2srv_dbus != NULL)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Problems2 service objects are already registered");
        return -EALREADY;
    }

    service->pv->p2srv_dbus = connection;

    service->pv->p2srv_p2_object = abrt_p2_object_new(service,
                                                      &(service->pv->p2srv_p2_type),
                                                      (char *)ABRT_P2_PATH,
                                                      /*node*/NULL,
                                                      /*node destructor*/NULL,
                                                      error);

    if (service->pv->p2srv_p2_object == 0)
    {
        g_prefix_error(error, "Failed to register Problems2 node: ");
        return -1;
    }

    struct bridge_call_args args;
    args.service = service;
    args.error = error;

    for_each_problem_in_dir(g_settings_dump_location, (uid_t)-1, bridge_register_dump_dir_entry_node, &args);

    if (*args.error != NULL)
    {
        g_prefix_error(error, "Failed to register Problems objects: ");
        return -1;
    }

    GError *local_error = NULL;
    service->pv->p2srv_proxy_dbus = g_dbus_proxy_new_sync(connection,
                                                          G_DBUS_PROXY_FLAGS_NONE,
                                                          NULL,
                                                          "org.freedesktop.DBus",
                                                          "/org/freedesktop/DBus",
                                                          "org.freedesktop.DBus",
                                                          NULL,
                                                          &local_error);


    if (local_error == NULL)
    {
        g_signal_connect(service->pv->p2srv_proxy_dbus,
                         "g-signal",
                         G_CALLBACK(on_g_signal),
                         service);
    }
    else
    {
        error_msg("Failed to initialize proxy to DBus: %s",
                  local_error->message);
        g_error_free(local_error);
    }

    return 0;
}

/*
 * Service configuration
 */
long abrt_p2_service_max_message_size(AbrtP2Service *service)
{
    return service->pv->p2srv_max_message_size;
}

void abrt_p2_service_set_max_message_size(AbrtP2Service *service,
            long max_message_size)
{
    service->pv->p2srv_max_message_size = max_message_size;
}

long abrt_p2_service_max_message_unix_fds(AbrtP2Service *service)
{
    return service->pv->p2srv_max_message_unix_fds;
}

void abrt_p2_service_set_max_message_unix_fds(AbrtP2Service *service,
            long max_message_unix_fds)
{
    service->pv->p2srv_max_message_unix_fds = max_message_unix_fds;
}

unsigned abrt_p2_service_user_clients_limit(AbrtP2Service *service,
            uid_t uid)
{
    return service->pv->p2srv_limit_clients;
}

void abrt_p2_service_set_user_clients_limit(AbrtP2Service *service,
            uid_t uid,
            unsigned limit)
{
    service->pv->p2srv_limit_clients = limit;
}

unsigned abrt_p2_service_elements_limit(AbrtP2Service *service,
            uid_t uid)
{
    return uid == 0 ? 0 : service->pv->p2srv_limit_elements;
}

void abrt_p2_service_set_elements_limit(AbrtP2Service *service,
            uid_t uid,
            unsigned limit)
{
    service->pv->p2srv_limit_elements = limit;
}

off_t abrt_p2_service_data_size_limit(AbrtP2Service *service,
            uid_t uid)
{
    return uid == 0 ? 0 : service->pv->p2srv_limit_data_size;
}

void abrt_p2_service_set_data_size_limit(AbrtP2Service *service,
            uid_t uid, off_t limit)
{
    service->pv->p2srv_limit_data_size = limit;
}

unsigned abrt_p2_service_user_problems_limit(AbrtP2Service *service,
            uid_t uid)
{
    return uid == 0 ? 0 : service->pv->p2srv_limit_user_problems;
}

void abrt_p2_service_set_user_problems_limit(AbrtP2Service *service,
            uid_t uid,
            unsigned limit)
{
    service->pv->p2srv_limit_user_problems = limit;
}

unsigned abrt_p2_service_new_problem_throttling_magnitude(AbrtP2Service *service,
            uid_t uid)
{
    return service->pv->p2srv_limit_new_problem_throttling_magnitude;
}

void abrt_p2_service_set_new_problem_throttling_magnitude(AbrtP2Service *service,
            uid_t uid,
            unsigned limit)
{
    service->pv->p2srv_limit_new_problem_throttling_magnitude = limit;
}

unsigned abrt_p2_service_new_problems_batch(AbrtP2Service *service,
            uid_t uid)
{
    return service->pv->p2srv_limit_new_problems_batch;
}

void abrt_p2_service_set_new_problems_batch(AbrtP2Service *service,
            uid_t uid,
            unsigned limit)
{
    service->pv->p2srv_limit_new_problems_batch = limit;
}

int abrt_p2_service_user_can_create_new_problem(AbrtP2Service *service,
            uid_t uid)
{
    if (uid == 0)
        return 1;

    time_t current = time(NULL);
    if (current == (time_t)-1)
    {
        perror_msg("time");
        return -1;
    }

    struct user_info *user = abrt_p2_service_user_lookup(service, uid);
    if (user == NULL)
    {
        error_msg("User does not have Session: uid=%lu",
                  (long unsigned)uid);
        return -1;
    }

    const unsigned upl = abrt_p2_service_user_problems_limit(service, uid);
    if (upl != 0 && user->problems >= upl)
        return -E2BIG;

    if (current < user->new_problem_last)
    {
        error_msg("The last problem was created in future: uid=%lu",
                  (long unsigned)uid);
        return -1;
    }

    /* Allows Y new problems to be created in a batch but then allow only 1 new
     * problem per Xs.
     *
     *  number of problems = minimum( ((last ts - current ts) / (2^magnitude)),
     *                                (configured number))
     */
    const long unsigned off = current - user->new_problem_last;
    const unsigned throttling = abrt_p2_service_new_problem_throttling_magnitude(service, uid);
    const long unsigned incr = (off >> throttling);

    const unsigned npb = abrt_p2_service_new_problems_batch(service, uid);
    /* Avoid overflow. Beware of adding operation inside the condition! */
    if (   incr > npb
        || (user->new_problems += incr) > npb)
        user->new_problems = npb;

    log_debug("NewProblem limit: last %lu, "
              "current %lu, "
              "increment %lu, "
              "remaining %u",
              (long unsigned)user->new_problem_last,
              (long unsigned)current,
              incr,
              user->new_problems);

    if (user->new_problems == 0)
        return 0;

    user->new_problem_last = current;
    return user->new_problems--;
}
