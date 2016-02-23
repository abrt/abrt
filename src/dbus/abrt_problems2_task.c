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

#include "abrt_problems2_task.h"

enum {
    SN_STATUS_CHANGED,
    SN_LAST_SIGNAL
} SignalNumber;

static guint s_signals[SN_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(AbrtP2Task, abrt_p2_task, G_TYPE_OBJECT)

static void abrt_p2_task_finalize(GObject *gobject)
{
    AbrtP2TaskPrivate *pv = abrt_p2_task_get_instance_private(ABRT_P2_TASK(gobject));
    g_variant_unref(pv->p2t_details);
    g_variant_unref(pv->p2t_results);
}

static void abrt_p2_task_class_init(AbrtP2TaskClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = abrt_p2_task_finalize;

    s_signals[SN_STATUS_CHANGED] = g_signal_new ("status-changed",
                                                 G_TYPE_FROM_CLASS (klass),
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET(AbrtP2TaskClass, status_changed),
                                                 /*accumulator*/NULL, /*accu_data*/NULL,
                                                 g_cclosure_marshal_VOID__INT,
                                                 G_TYPE_NONE,
                                                 /*n_params*/1,
                                                 G_TYPE_INT);
}

#define ABRT_P2_TASK_ABSTRACT_FUNCTION_CALL(method, task, ...) \
    do { if (ABRT_P2_TASK_GET_CLASS(task)->method == NULL) { \
            error_msg(); \
            abort("Undefined Task abstract function "#method); \
         }\
         ABRT_P2_TASK_GET_CLASS(task)->method(task, __VA_ARGS__); } while(0)

#define ABRT_P2_TASK_VIRTUAL_FUNCTION_CALL(method, task, ...) \
    do { if (ABRT_P2_TASK_GET_CLASS(task)->method) \
            ABRT_P2_TASK_GET_CLASS(task)->method(task, __VA_ARGS__); } while(0)

#define ABRT_P2_TASK_VIRTUAL_CANCEL(task, error) \
    ABRT_P2_TASK_VIRTUAL_FUNCTION_CALL(cancel, task, error)

#define ABRT_P2_TASK_VIRTUAL_FINISH(task, error) \
    ABRT_P2_TASK_VIRTUAL_FUNCTION_CALL(finish, task, error)

#define ABRT_P2_TASK_VIRTUAL_START(task, options, error) \
    ABRT_P2_TASK_VIRTUAL_FUNCTION_CALL(start, task, options, error)

static void abrt_p2_task_init(AbrtP2Task *self)
{
    self->pv = abrt_p2_task_get_instance_private(self);
    self->pv->p2t_details = g_variant_new("a{sv}", NULL);
}

static void abrt_p2_task_change_status(AbrtP2Task *task,
            AbrtP2TaskStatus status)
{
    if (task->pv->p2t_status == status)
        return;

    task->pv->p2t_status = status;

    g_signal_emit(task,
                  s_signals[SN_STATUS_CHANGED],
                  0,
                  status);
}

AbrtP2TaskStatus abrt_p2_task_status(AbrtP2Task *task)
{
    return task->pv->p2t_status;
}

GVariant *abrt_p2_task_details(AbrtP2Task *task)
{
    return g_variant_ref(task->pv->p2t_details);
}

void abrt_p2_task_add_detail(AbrtP2Task *task,
            const char *key,
            GVariant *value)
{
    GVariantDict dict;
    g_variant_dict_init(&dict, task->pv->p2t_details);
    g_variant_dict_insert(&dict, key, "v", value);

    if (task->pv->p2t_details)
        g_variant_unref(task->pv->p2t_details);

    task->pv->p2t_details = g_variant_dict_end(&dict);
}

void abrt_p2_task_set_response(AbrtP2Task *task,
            GVariant *response)
{
    if (task->pv->p2t_results != NULL)
        log_warning("Task already has response assigned");

    task->pv->p2t_results = response;
}

bool abrt_p2_task_is_cancelled(AbrtP2Task *task)
{
    return (task->pv->p2t_cancellable
              && g_cancellable_is_cancelled(task->pv->p2t_cancellable))
           || task->pv->p2t_status == ABRT_P2_TASK_STATUS_CANCELED;
}

void abrt_p2_task_cancel(AbrtP2Task *task,
            GError **error)
{
    if (abrt_p2_task_is_cancelled(task))
        return;

    if (task->pv->p2t_status == ABRT_P2_TASK_STATUS_RUNNING)
        g_cancellable_cancel(task->pv->p2t_cancellable);
    else if (task->pv->p2t_status == ABRT_P2_TASK_STATUS_STOPPED)
    {
        ABRT_P2_TASK_VIRTUAL_CANCEL(task, error);

        if (*error == NULL)
            abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_CANCELED);
    }
    else
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Task is not in the state that allows cancelling");
}

void abrt_p2_task_finish(AbrtP2Task *task,
            GVariant **result,
            gint32 *code,
            GError **error)
{
    if (   task->pv->p2t_status != ABRT_P2_TASK_STATUS_DONE
        && task->pv->p2t_status != ABRT_P2_TASK_STATUS_FAILED)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                "Cannot finalize undone task");
        return;
    }

    ABRT_P2_TASK_VIRTUAL_FINISH(task, error);

    if (*error != NULL)
        return;

    if (task->pv->p2t_results)
        *result = g_variant_ref(task->pv->p2t_results);
    else
        *result = g_variant_new("a{sv}", NULL);

    *code = task->pv->p2t_code;
}

static void abrt_p2_task_finish_gtask(GObject *source_object,
           GAsyncResult *result,
           gpointer user_data)
{
    AbrtP2Task *task = ABRT_P2_TASK(source_object);

    if (!g_task_is_valid(result, task))
    {
        error_msg("BUG:%s:%s: invalid GTask", __FILE__, __func__);
        return;
    }

    GError *error = NULL;
    const gint32 code = g_task_propagate_int(G_TASK(result), &error);

    if (code == ABRT_P2_TASK_CODE_STOP)
    {
        log_debug("Task stopped");

        abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_STOPPED);
    }
    else if (code >= ABRT_P2_TASK_CODE_DONE)
    {
        log_debug("Task done");

        task->pv->p2t_code = code - ABRT_P2_TASK_CODE_DONE;
        abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_DONE);
    }
    else if (abrt_p2_task_is_cancelled(task))
    {
        if (error != NULL)
        {
            log_debug("Task canceled with error: %s", error->message);

            g_error_free(error);
            error = NULL;
        }
        else
            log_debug("Task canceled");

        ABRT_P2_TASK_VIRTUAL_CANCEL(task, &error);
        abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_CANCELED);
    }
    else
    {
        GVariantDict response;
        g_variant_dict_init(&response, NULL);

        if (error != NULL)
        {
            log_debug("Task failed with error: %s", error->message);

            g_variant_dict_insert(&response,
                                  "Error.Message",
                                  "s",
                                  error->message);

            g_error_free(error);
        }
        else if (code == ABRT_P2_TASK_CODE_ERROR)
        {
            log_debug("Task failed without error message");

            g_variant_dict_insert(&response,
                                  "Error.Message",
                                  "s",
                                  "Task failed");
        }
        else
        {
            error_msg("BUG:%s:%s: unknown Task return code: %d",
                      __FILE__,
                      __func__,
                      code);

            g_variant_dict_insert(&response,
                                  "Error.Message",
                                  "s",
                                  "Internal error: Invalid Task return code");
        }

        abrt_p2_task_set_response(task, g_variant_dict_end(&response));
        abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_FAILED);
    }

    g_object_unref(task->pv->p2t_cancellable);
    task->pv->p2t_cancellable = NULL;
}

static void abrt_p2_task_thread(GTask *task,
            gpointer source_object,
            gpointer task_data,
            GCancellable *cancellable)
{
    AbrtP2Task *stask = source_object;

    GError *error = NULL;
    gint32 code = ABRT_P2_TASK_GET_CLASS(stask)->run(stask, &error);

    if (error == NULL)
        g_task_return_int(task, code);
    else
        g_task_return_error(task, error);
}

void abrt_p2_task_start(AbrtP2Task *task,
            GVariant *options,
            GError **error)
{
    if (   task->pv->p2t_status != ABRT_P2_TASK_STATUS_NEW
        && task->pv->p2t_status != ABRT_P2_TASK_STATUS_STOPPED)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Cannot start task that is not new or stopped");
        return;
    }

    ABRT_P2_TASK_VIRTUAL_START(task, options, error);

    if (*error != NULL)
        return;

    task->pv->p2t_cancellable = g_cancellable_new();
    GTask *gtask = g_task_new(task,
                              task->pv->p2t_cancellable,
                              abrt_p2_task_finish_gtask,
                              NULL);

    g_task_run_in_thread(gtask, abrt_p2_task_thread);

    abrt_p2_task_change_status(task, ABRT_P2_TASK_STATUS_RUNNING);
    g_object_unref(gtask);
}

static void abrt_p2_task_autonomous_cb(AbrtP2Task *task,
            gint32 status,
            gpointer user_data)
{
    switch(status)
    {
        case ABRT_P2_TASK_STATUS_NEW:
            error_msg("Autonomous task has changed status to NEW");
            break;

        case ABRT_P2_TASK_STATUS_RUNNING:
            log_debug("Autonomous task has successfully started");
            break;

        case ABRT_P2_TASK_STATUS_STOPPED:
            {
                error_msg("Autonomous task has been stopped and will be canceled");

                GError *error = NULL;
                abrt_p2_task_cancel(task, &error);
                if (error != NULL)
                {
                    error_msg("Failed to cancel stopped autonomous task: %s",
                               error->message);

                    g_error_free(error);
                    g_object_unref(task);
                }
            }
            break;

        case ABRT_P2_TASK_STATUS_CANCELED:
            log_notice("Autonomous task has been canceled");
            g_object_unref(task);
            break;

        case ABRT_P2_TASK_STATUS_FAILED:
            {
                GVariant *response;
                gint32 code;
                GError *error = NULL;
                abrt_p2_task_finish(task, &response, &code, &error);
                if (error != NULL)
                {
                    error_msg("Failed to finish canceled task: %s", error->message);
                }
                else
                {
                    const char *error_message;
                    g_variant_lookup(response, "Error.Message", "&s", &error_message);
                    error_msg("Autonomous task has failed: %d: %s", code, error_message);
                    g_variant_unref(response);
                }

                g_object_unref(task);
            }
            break;

        case ABRT_P2_TASK_STATUS_DONE:
            log_notice("Autonomous task has been successfully finished");
            g_object_unref(task);
            break;

        default:
            error_msg("BUG: %s: forgotten task state", __func__);
            break;
    }
}

void abrt_p2_task_autonomous_run(AbrtP2Task *task,
        GError **error)
{
    g_signal_connect(task,
                     "status-changed",
                     G_CALLBACK(abrt_p2_task_autonomous_cb),
                     NULL);

    abrt_p2_task_start(task, NULL, error);
}
