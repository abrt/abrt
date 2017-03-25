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

#include "abrt_problems2_session.h"

#include "libabrt.h"

static PolkitAuthority *s_pk_authority;

PolkitAuthority *abrt_p2_session_class_set_polkit_authority(PolkitAuthority *pk_authority)
{
    if (s_pk_authority != NULL)
    {
        log_warning("Session: polkit Authority already initialized");

        /*
         * Introduce something like this to libreport
        if (g_verbose > 3)
            abort();
        */
        return s_pk_authority;
    }

    s_pk_authority = pk_authority;

    return s_pk_authority;
}

PolkitAuthority *abrt_p2_session_class_polkit_authority(void)
{
    if (s_pk_authority == NULL)
        log_debug("Session: Polkit Authority not-yet initialized");

    return s_pk_authority;
}

PolkitAuthority *abrt_p2_session_class_release_polkit_authority(void)
{
    PolkitAuthority *pk_authority = s_pk_authority;
    s_pk_authority = NULL;
    return pk_authority;
}

typedef struct
{
    char     *p2s_caller;
    uid_t    p2s_uid;
    int      p2s_state;
    GList    *p2s_tasks;
    uint32_t p2s_task_indexer;
    GHashTable *p2s_tokens;
    struct check_auth_cb_params *p2s_auth_rq;
    PolkitSubject *p2s_pk_subject;
} AbrtP2SessionPrivate;

enum
{
    ABRT_P2_SESSION_STATE_INIT,
    ABRT_P2_SESSION_STATE_PENDING,
    ABRT_P2_SESSION_STATE_AUTH,
};

struct _AbrtP2Session
{
    GObject parent_instance;
    AbrtP2SessionPrivate *pv;
};

G_DEFINE_TYPE_WITH_PRIVATE(AbrtP2Session, abrt_p2_session, G_TYPE_OBJECT)

struct check_auth_cb_params
{
    AbrtP2Session *session;
    GCancellable *cancellable;
    PolkitDetails *details;
};

enum {
    SN_AUTHORIZATION_CHANGED,
    SN_LAST_SIGNAL
} SignalNumber;

static guint s_signals[SN_LAST_SIGNAL] = { 0 };

static void abrt_p2_session_finalize(GObject *gobject)
{
    AbrtP2SessionPrivate *pv = abrt_p2_session_get_instance_private(ABRT_P2_SESSION(gobject));
    free(pv->p2s_caller);
    g_hash_table_destroy(pv->p2s_tokens);

    /* If there is ongoing authorization, */
    /* tell the callback that it must not touch session.*/
    if (pv->p2s_auth_rq != NULL)
        pv->p2s_auth_rq->session = NULL;

    if (pv->p2s_pk_subject != NULL)
        g_object_unref(pv->p2s_pk_subject);
}

static void abrt_p2_session_class_init(AbrtP2SessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = abrt_p2_session_finalize;

    s_signals[SN_AUTHORIZATION_CHANGED] = g_signal_new ("authorization-changed",
                                                        G_TYPE_FROM_CLASS (klass),
                                                        G_SIGNAL_RUN_LAST,
                                                        G_STRUCT_OFFSET(AbrtP2SessionClass, authorization_changed),
                                                        /*accumulator*/NULL, /*accu_data*/NULL,
                                                        g_cclosure_marshal_VOID__INT,
                                                        G_TYPE_NONE,
                                                        /*n_params*/1,
                                                        G_TYPE_INT);
}

static void abrt_p2_session_init(AbrtP2Session *self)
{
    self->pv = abrt_p2_session_get_instance_private(self);
}

static void emit_authorization_changed(AbrtP2Session *session,
            AbrtP2SessionAuthChangedStatus status)
{
    g_signal_emit(session,
                  s_signals[SN_AUTHORIZATION_CHANGED],
                  0,
                  (gint32)status);
}

static void change_state(AbrtP2Session *session, int new_state)
{
    if (session->pv->p2s_state == new_state)
        return;

    AbrtP2SessionAuthChangedStatus value = -1;
    int old_state = session->pv->p2s_state;
    session->pv->p2s_state = new_state;

    if      (old_state == ABRT_P2_SESSION_STATE_INIT    && new_state == ABRT_P2_SESSION_STATE_PENDING)
    {
        log_debug("Authorization request is pending");
        value = ABRT_P2_SESSION_CHANGED_PENDING;
    }
    else if (old_state == ABRT_P2_SESSION_STATE_INIT    && new_state == ABRT_P2_SESSION_STATE_AUTH)
    {
        log_debug("Authorization has been granted");
        value = ABRT_P2_SESSION_CHANGED_AUTHORIZED;
    }
    else if (old_state == ABRT_P2_SESSION_STATE_PENDING && new_state == ABRT_P2_SESSION_STATE_AUTH)
    {
        log_debug("Authorization has been acquired");
        value = ABRT_P2_SESSION_CHANGED_AUTHORIZED;
    }
    else if (old_state == ABRT_P2_SESSION_STATE_AUTH    && new_state == ABRT_P2_SESSION_STATE_INIT)
    {
        log_debug("Authorization request has been lost");
        value = ABRT_P2_SESSION_CHANGED_NOT_AUTHORIZED;
    }
    else if (old_state == ABRT_P2_SESSION_STATE_PENDING && new_state == ABRT_P2_SESSION_STATE_INIT)
    {
        log_debug("Authorization request has failed");
        value = ABRT_P2_SESSION_CHANGED_FAILED;
    }
    else
        goto forgotten_state;

    emit_authorization_changed(session, value);
    return;

forgotten_state:
    error_msg("BUG: unsupported state, current : %d, new : %d",
              session->pv->p2s_state,
              new_state);
}

static void check_authorization_callback(GObject *source,
            GAsyncResult *res,
            gpointer user_data)
{
    GError *error = NULL;
    PolkitAuthorizationResult *result = NULL;
    result = polkit_authority_check_authorization_finish(POLKIT_AUTHORITY(source),
                                                         res,
                                                         &error);

    int new_state = ABRT_P2_SESSION_STATE_INIT;
    if (result == NULL)
    {
       error_msg("Polkit authorization failed: %s", error->message);
       g_error_free(error);
    }
    else
    {
        if (polkit_authorization_result_get_is_authorized(result))
            new_state = ABRT_P2_SESSION_STATE_AUTH;
        else
            /* We do not support polkit_authorization_result_get_is_challenge */
            log_debug("Not authorized");

        g_object_unref(result);
    }

    struct check_auth_cb_params *params = (struct check_auth_cb_params *)user_data;
    AbrtP2Session *session = params->session;
    g_object_unref(params->cancellable);

    if (params->details != NULL)
        g_object_unref(params->details);

    free(params);

    /* It might happen that the session had been destroyed before the callback
     * was called (e.g. if you programmatically cancel the operation, this
     * callback is called in the next main loop iteration). */
    if (session != NULL)
    {
        change_state(session, new_state);
        session->pv->p2s_auth_rq = NULL;
    }
    else
        log_debug("Operation finished after the session had been destroyed");
}

static void authorization_request_initialize(AbrtP2Session *session, GVariant *parameters)
{
    struct check_auth_cb_params *auth_rq = xmalloc(sizeof(*auth_rq));
    auth_rq->session = session;
    auth_rq->cancellable = g_cancellable_new();

    auth_rq->details = NULL;
    if (parameters != NULL)
    {
        GVariant *message = g_variant_lookup_value(parameters,
                                                   "message",
                                                   G_VARIANT_TYPE_STRING);

        if (message != NULL)
        {
            auth_rq->details = polkit_details_new();
            polkit_details_insert(auth_rq->details,
                                  "polkit.message",
                                  g_variant_get_string(message,
                                                       NULL));

            g_variant_unref(message);
        }
    }

    session->pv->p2s_auth_rq = auth_rq;
    change_state(session, ABRT_P2_SESSION_STATE_PENDING);

    /* http://www.freedesktop.org/software/polkit/docs/latest/polkit-apps.html
     */
    if (session->pv->p2s_pk_subject == NULL)
        session->pv->p2s_pk_subject = polkit_system_bus_name_new(session->pv->p2s_caller);

    /* If you cancel this operation programmatically, you might get the
     * following warning message:
         WARNING **: Error cancelling authorization check:
         GDBus.Error:org.freedesktop.PolicyKit1.Error.Failed:
         No such cancellation_id `cancellation-id-1' for name :1.257

       The message is returned by polkitbackend and it just means that there
       were nothing to cancel. Probably because the operation was already
       finished or not-yet started.
       */
    polkit_authority_check_authorization(abrt_p2_session_class_polkit_authority(),
                                         session->pv->p2s_pk_subject,
                                         "org.freedesktop.problems.getall",
                                         auth_rq->details,
                                         POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                         auth_rq->cancellable,
                                         check_authorization_callback,
                                         auth_rq);

}

AbrtP2Session *abrt_p2_session_new(char *caller, uid_t uid)
{
    AbrtP2Session *session = g_object_new(TYPE_ABRT_P2_SESSION, NULL);
    session->pv->p2s_caller = caller;
    session->pv->p2s_uid = uid;

    if (session->pv->p2s_uid == 0)
        session->pv->p2s_state = ABRT_P2_SESSION_STATE_AUTH;
    else
        session->pv->p2s_state = ABRT_P2_SESSION_STATE_INIT;

    session->pv->p2s_tokens = g_hash_table_new_full(g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    NULL);

    return session;
}

const char *abrt_p2_session_generate_token(AbrtP2Session *session,
            unsigned int duration,
            GError **error)
{
    if (session->pv->p2s_state != ABRT_P2_SESSION_STATE_AUTH)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Session is not authorized");
        return NULL;
    }

#define SESSION_TOKEN_LENGTH 16
    static const char *const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (duration == 0)
        duration = 5;

    FILE *urandom = fopen("/dev/urandom", "rb");

    if (urandom == NULL)
    {
        perror_msg("fopen(/dev/urandom, rb)");
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to open /dev/urandom for reading");
        return NULL;
    }

    unsigned int seed = 0;
    const size_t r = fread(&seed, 1, sizeof(seed), urandom);
    fclose(urandom);

    if (sizeof(seed) != r)
    {
        perror_msg("fread(unsigned int, /dev/urandom)");
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to read 'unsigned int' from /dev/urandom");
        return NULL;
    }

    char *token = xmalloc((SESSION_TOKEN_LENGTH + 1) * sizeof(char));
    for (char *iter = token; iter < token + SESSION_TOKEN_LENGTH; ++iter)
        *iter = alphabet[(int)(strlen(alphabet) * (rand_r(&seed) / (double)RAND_MAX))];

    token[SESSION_TOKEN_LENGTH] = '\0';

    const time_t curtime = time(NULL);
    if (curtime == ((time_t) -1))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Cannot get current time");
        free(token);
        return NULL;
    }

    g_hash_table_insert(session->pv->p2s_tokens,
                        token,
                        GINT_TO_POINTER(curtime + duration));

    return token;
#undef SESSION_TOKEN_LENGTH
}

int abrt_p2_session_revoke_token(AbrtP2Session *session,
            const char *token)
{
    return g_hash_table_remove(session->pv->p2s_tokens, token) ? 0 : 1;
}

static AbrtP2SessionAuthRequestRet abrt_p2_session_authorize_peer_with_token(
            AbrtP2Session *session,
            AbrtP2Session *peer_session,
            const char *token,
            GError **error)
{
    if (session->pv->p2s_state != ABRT_P2_SESSION_STATE_AUTH)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "Not authorized session cannot pass authorization");
        return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

    if (session->pv->p2s_uid != peer_session->pv->p2s_uid)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "Session owners do not match");
        return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

    const gpointer expire = g_hash_table_lookup(session->pv->p2s_tokens, token);
    if (expire == NULL)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "No such token");
        return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

    const time_t curtime = time(NULL);
    if (curtime == ((time_t) -1))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Cannot get current time");
        return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

    if (curtime > (time_t)(GPOINTER_TO_INT(expire)))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "Token has already expired");
        return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

    g_hash_table_remove(session->pv->p2s_tokens, token);
    abrt_p2_session_grant_authorization(peer_session);

    log_info("Granted authorization to peer session on '%s' bus",
             peer_session->pv->p2s_caller);

    return ABRT_P2_SESSION_AUTHORIZE_GRANTED;
}

static int abrt_p2_session_cmp_caller(AbrtP2Session *lhs, const char *bus_name)
{
    return strcmp(lhs->pv->p2s_caller, bus_name);
}

static AbrtP2SessionAuthRequestRet _abrt_p2_session_begin_auth(AbrtP2Session *session,
            GVariant *parameters,
            GList *peers,
            GError **error)
{
    AbrtP2SessionAuthRequestRet ret = ABRT_P2_SESSION_AUTHORIZE_FAILED;

    GVariant *peer_bus = g_variant_lookup_value(parameters,
                                                "problems2.peer-bus",
                                                G_VARIANT_TYPE_STRING);

    GVariant *peer_token = g_variant_lookup_value(parameters,
                                             "problems2.peer-token",
                                             G_VARIANT_TYPE_STRING);

    if (!peer_bus && !peer_token)
    {
        authorization_request_initialize(session, parameters);
        ret = ABRT_P2_SESSION_AUTHORIZE_ACCEPTED;
    }
    else if (peer_bus && peer_token)
    {
        const gchar *bus = g_variant_get_string(peer_bus, NULL);
        GList *tmp = g_list_find_custom(peers,
                                        bus,
                                        (GCompareFunc)abrt_p2_session_cmp_caller);
        if (tmp == NULL)
        {
            g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "No peer session for bus '%s'",
                        g_variant_get_string(peer_bus, NULL));
            goto finito;
        }

        AbrtP2Session *peer_session = (AbrtP2Session *)tmp->data;
        const gchar *token = g_variant_get_string(peer_token, NULL);
        ret = abrt_p2_session_authorize_peer_with_token(peer_session,
                                                        session,
                                                        token,
                                                        error);
    }
    else /* if ((peer_bus && !peer_token) || (!peer_bus && peer_token)) */
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Invalid parameters peer-bus and peer-token.");
    }

finito:
    if (peer_bus != NULL)
        g_variant_unref(peer_bus);

    if (peer_token != NULL)
        g_variant_unref(peer_token);

    return ret;
}

AbrtP2SessionAuthRequestRet abrt_p2_session_authorize(AbrtP2Session *session,
            GVariant *parameters,
            GList *peers,
            GError **error)
{
    switch(session->pv->p2s_state)
    {
        case ABRT_P2_SESSION_STATE_INIT:
            return _abrt_p2_session_begin_auth(session, parameters, peers, error);

        case ABRT_P2_SESSION_STATE_PENDING:
            return ABRT_P2_SESSION_AUTHORIZE_PENDING;

        case ABRT_P2_SESSION_STATE_AUTH:
            return ABRT_P2_SESSION_AUTHORIZE_GRANTED;

        default:
            error_msg("BUG: %s: forgotten state -> %d", __func__, session->pv->p2s_state);
            return ABRT_P2_SESSION_AUTHORIZE_FAILED;
    }

}

void abrt_p2_session_revoke_authorization(AbrtP2Session *session)
{
    if (session->pv->p2s_uid == 0)
        return;

    switch(session->pv->p2s_state)
    {
        case ABRT_P2_SESSION_STATE_AUTH:
            change_state(session, ABRT_P2_SESSION_STATE_INIT);
            break;

        case ABRT_P2_SESSION_STATE_PENDING:
            g_cancellable_cancel(session->pv->p2s_auth_rq->cancellable);
            change_state(session, ABRT_P2_SESSION_STATE_INIT);
            break;

        case ABRT_P2_SESSION_STATE_INIT:
            /* pass */
            break;
    }
}

AbrtP2SessionAuthRequestRet abrt_p2_session_grant_authorization(AbrtP2Session *session)
{
    switch(session->pv->p2s_state)
    {
        case ABRT_P2_SESSION_STATE_AUTH:
            /* pass */
            break;

        case ABRT_P2_SESSION_STATE_PENDING:
            g_cancellable_cancel(session->pv->p2s_auth_rq->cancellable);
            change_state(session, ABRT_P2_SESSION_STATE_AUTH);
            break;

        case ABRT_P2_SESSION_STATE_INIT:
            change_state(session, ABRT_P2_SESSION_STATE_AUTH);
            break;
    }

    return ABRT_P2_SESSION_AUTHORIZE_GRANTED;
}

uid_t abrt_p2_session_uid(AbrtP2Session *session)
{
    return session->pv->p2s_uid;
}

const char *abrt_p2_session_caller(AbrtP2Session *session)
{
    return session->pv->p2s_caller;
}

int abrt_p2_session_is_authorized(AbrtP2Session *session)
{
    return session->pv->p2s_state == ABRT_P2_SESSION_STATE_AUTH;
}

int abrt_p2_session_check_sanity(AbrtP2Session *session,
            const char *caller,
            uid_t caller_uid,
            GError **error)
{
    if (strcmp(session->pv->p2s_caller, caller) == 0 && session->pv->p2s_uid == caller_uid)
        /* the session node is sane */
        return 0;

    log_warning("Problems2 Session object does not belong to UID %d", caller_uid);

    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                "Your Problems2 Session is broken. Check system logs for more details.");
    return -1;
}

uint32_t abrt_p2_session_add_task(AbrtP2Session *session,
            AbrtP2Task *task,
            GError **error)
{
    if (session->pv->p2s_task_indexer == (UINT32_MAX - 1))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Reached the limit of task per session.");

        return UINT32_MAX;
    }

    if (abrt_p2_session_owns_task(session, task) == 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Task is already owned by the session");

        return UINT32_MAX;
    }

    session->pv->p2s_tasks = g_list_prepend(session->pv->p2s_tasks, task);

    return session->pv->p2s_task_indexer++;
}

void abrt_p2_session_remove_task(AbrtP2Session *session,
            AbrtP2Task *task,
            GError **error)
{
    session->pv->p2s_tasks = g_list_remove(session->pv->p2s_tasks, task);
}

GList *abrt_p2_session_tasks(AbrtP2Session *session)
{
    return session->pv->p2s_tasks;
}

int abrt_p2_session_owns_task(AbrtP2Session *session,
            AbrtP2Task *task)
{
    return !(g_list_find(session->pv->p2s_tasks, task));
}

int abrt_p2_session_tasks_count(AbrtP2Session *session)
{
    return g_list_length(session->pv->p2s_tasks);
}

static void abrt_p2_session_dispose_task(AbrtP2Task *task,
            gint32 status)
{
    switch(status)
    {
        case ABRT_P2_TASK_STATUS_STOPPED:
            {
                GError *local_error = NULL;
                abrt_p2_task_cancel(task, &local_error);
                if (local_error != NULL)
                {
                    error_msg("Task garbage collector failed to cancel task: %s",
                              local_error->message);

                    g_error_free(local_error);
                }

                /* In case of errors, this could cause problems, but I
                 * don't have better plan yet. */
                log_debug("Disposed new/stopped task: %p", task);
                g_object_unref(task);
            }
            break;

        case ABRT_P2_TASK_STATUS_NEW:
            log_debug("Disposed new task: %p", task);
            g_object_unref(task);
            break;

        case ABRT_P2_TASK_STATUS_FAILED:
            log_debug("Disposed failed task: %p", task);
            g_object_unref(task);
            break;

        case ABRT_P2_TASK_STATUS_CANCELED:
            log_debug("Disposed canceled task: %p", task);
            g_object_unref(task);
            break;

        case ABRT_P2_TASK_STATUS_DONE:
            log_debug("Disposed done task: %p", task);
            g_object_unref(task);
            break;

        case ABRT_P2_TASK_STATUS_RUNNING:
            error_msg("BUG: cannot dispose RUNNING task");
            abort();
            break;
    }
}

static void abrt_p2_session_delayed_dispose_task(AbrtP2Task *task,
            gint32 status,
            gpointer user_data)
{
    if (status == ABRT_P2_TASK_STATUS_RUNNING)
    {
        error_msg("BUG: task to dispose must not change state to RUNNING");

        abort();
    }

    log_debug("Going to dispose delayed task: %p: %d",
              task,
              status);

    abrt_p2_session_dispose_task(task, status);
}

void abrt_p2_session_clean_tasks(AbrtP2Session *session)
{
    GList *task = session->pv->p2s_tasks;
    session->pv->p2s_tasks = NULL;

    while (task != NULL)
    {
        AbrtP2Task *t = ABRT_P2_TASK(task->data);

        task = g_list_delete_link(task, task);

        const AbrtP2TaskStatus status = abrt_p2_task_status(t);
        if (status != ABRT_P2_TASK_STATUS_RUNNING)
        {
            abrt_p2_session_dispose_task(t, status);
            continue;
        }

        log_debug("Delaying disposal of running task: %p", t);

        g_signal_connect(t,
                         "status-changed",
                         G_CALLBACK(abrt_p2_session_delayed_dispose_task),
                         NULL);

        GError *local_error = NULL;
        abrt_p2_task_cancel(t, &local_error);

        if (local_error != NULL)
        {
            error_msg("Task garbage collector failed to cancel running task: %s",
                      local_error->message);

            g_error_free(local_error);
        }
    }
}
