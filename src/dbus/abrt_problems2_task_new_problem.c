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

#include "abrt_problems2_task_new_problem.h"
#include "abrt_problems2_entry.h"

typedef struct
{
    AbrtP2Service *p2tnp_service;
    GVariant *p2tnp_problem_info;
    uid_t p2tnp_caller_uid;
    GUnixFDList *p2tnp_fd_list;
    AbrtP2Object *p2tnp_obj;        ///<< AbrtProblems2Entry
    bool p2tnp_wait_before_notify;
} AbrtP2TaskNewProblemPrivate;

struct _AbrtP2TaskNewProblem
{
    AbrtP2Task parent_instance;
    AbrtP2TaskNewProblemPrivate *pv;
};

static AbrtP2TaskCode abrt_p2_task_new_problem_run(AbrtP2Task *task,
            GError **error);

G_DEFINE_TYPE_WITH_PRIVATE(AbrtP2TaskNewProblem, abrt_p2_task_new_problem, ABRT_TYPE_P2_TASK)

static void abrt_p2_task_new_problem_finalize(GObject *gobject)
{
    AbrtP2TaskNewProblemPrivate *pv = abrt_p2_task_new_problem_get_instance_private(ABRT_P2_TASK_NEW_PROBLEM(gobject));
    g_variant_unref(pv->p2tnp_problem_info);

    if (pv->p2tnp_fd_list)
        g_object_unref(pv->p2tnp_fd_list);

    G_OBJECT_CLASS(abrt_p2_task_new_problem_parent_class)->finalize(gobject);
}

static void abrt_p2_task_remove_temporary_entry(AbrtP2TaskNewProblem *task,
            GError **error)
{
    if (task->pv->p2tnp_obj == NULL)
        return;

    AbrtP2Entry *entry = abrt_p2_object_get_node(task->pv->p2tnp_obj);

    log_debug("Task '%p': Removing temporary entry: %s",
              task,
              abrt_p2_entry_problem_id(entry));

    abrt_p2_entry_delete(entry,
                         /* act as super user to allow us to delete the temporary dir */0,
                         error);

    abrt_p2_object_destroy(task->pv->p2tnp_obj);

    task->pv->p2tnp_obj = NULL;
}

static void abrt_p2_task_new_problem_cancel(AbrtP2Task *task,
            GError **error)
{
    abrt_p2_task_remove_temporary_entry(ABRT_P2_TASK_NEW_PROBLEM(task), error);
}

static void abrt_p2_task_new_problem_class_init(AbrtP2TaskNewProblemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = abrt_p2_task_new_problem_finalize;

    AbrtP2TaskClass *task_class = ABRT_P2_TASK_CLASS(klass);
    task_class->run = abrt_p2_task_new_problem_run;
    task_class->cancel = abrt_p2_task_new_problem_cancel;
}

static void abrt_p2_task_new_problem_init(AbrtP2TaskNewProblem *self)
{
    self->pv = abrt_p2_task_new_problem_get_instance_private(self);
}

AbrtP2TaskNewProblem *abrt_p2_task_new_problem_new(AbrtP2Service *service,
            GVariant *problem_info,uid_t caller_uid,
            GUnixFDList *fd_list)
{
    AbrtP2TaskNewProblem *task = g_object_new(TYPE_ABRT_P2_TASK_NEW_PROBLEM, NULL);

    task->pv->p2tnp_service = service;
    task->pv->p2tnp_problem_info = problem_info;
    task->pv->p2tnp_caller_uid = caller_uid;
    task->pv->p2tnp_fd_list = fd_list;
    task->pv->p2tnp_wait_before_notify = false;

    return task;
}

void abrt_p2_task_new_problem_wait_before_notify(AbrtP2TaskNewProblem *task,
            bool value)
{
    task->pv->p2tnp_wait_before_notify = value;
}

static AbrtP2Object *abrt_p2_task_new_problem_create_directory_task(AbrtP2TaskNewProblem *task,
            GError **error)
{
    int r = abrt_p2_service_user_can_create_new_problem(task->pv->p2tnp_service, task->pv->p2tnp_caller_uid);
    if (r == 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                    "Too many problems have been recently created");

        return NULL;
    }
    if (r == -E2BIG)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                    "No more problems can be created");

        return NULL;
    }
    if (r < 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to check NewProblem limits");

        return NULL;
    }

    char *problem_id = abrt_p2_service_save_problem(task->pv->p2tnp_service,
                                                    task->pv->p2tnp_problem_info,
                                                    task->pv->p2tnp_fd_list,
                                                    task->pv->p2tnp_caller_uid, error);
    if (*error != NULL)
        return NULL;

    AbrtP2Entry *entry = abrt_p2_entry_new_with_state(problem_id,
                                                      ABRT_P2_ENTRY_STATE_NEW);

    AbrtP2Object *obj = abrt_p2_service_register_entry(task->pv->p2tnp_service,
                                                       entry,
                                                       error);

    if (*error != NULL)
        return NULL;

    return obj;
}

static int abrt_p2_task_new_problem_notify_directory_task(AbrtP2TaskNewProblem *task,
            char **new_path, GError **error)
{
    AbrtP2Entry *entry = abrt_p2_object_get_node(task->pv->p2tnp_obj);

    char *message = NULL;
    const char *problem_id = abrt_p2_entry_problem_id(entry);
    int r = notify_new_path_with_response(problem_id, &message);
    if (r < 0)
    {
        log_debug("Task '%p': Failed to communicate with the problems daemon",
                  task);

        GError *local_error = NULL;
        abrt_p2_entry_delete(entry,
                             /* allow us to delete the temporary dir */0,
                             &local_error);

        if (local_error != NULL)
        {
            error_msg("Can't remove temporary problem directory: %s",
                      local_error->message);

            g_error_free(local_error);
        }

        abrt_p2_object_destroy(task->pv->p2tnp_obj);
        task->pv->p2tnp_obj = NULL;

        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to notify the new problem directory");

        return r;
    }

    gint32 code;
    log_debug("Task '%p': New path processed: %d", task, r);
    if (r == 303)
    {
        /* 303 - the daemon found a local duplicate problem */

        abrt_p2_object_destroy(task->pv->p2tnp_obj);
        task->pv->p2tnp_obj = NULL;

        AbrtP2Object *obj = abrt_p2_service_get_entry_for_problem(task->pv->p2tnp_service,
                                                                  message,
                                                                  ABRT_P2_SERVICE_ENTRY_LOOKUP_NOFLAGS,
                                                                  error);
        if (obj == NULL)
        {
            error_msg("Problem Entry for directory '%s' does not exist", message);
            free(message);
            return ABRT_P2_TASK_CODE_ERROR;
        }

        *new_path = xstrdup(abrt_p2_object_path(obj));
        code = ABRT_P2_TASK_NEW_PROBLEM_DUPLICATE;

        log_debug("Task '%p': New occurrence of '%s'",
                  task,
                  *new_path);

        /* TODO: what about to teach the service to understand task's signals? */
        abrt_p2_service_notify_entry_object(task->pv->p2tnp_service,
                                            obj,
                                            error);
    }
    else if (r == 410)
    {
        /* 410 - the problem was refused by the daemon */
        log_debug("Task '%p': Problem dropped", task);

        abrt_p2_object_destroy(task->pv->p2tnp_obj);
        task->pv->p2tnp_obj = NULL;

        code = ABRT_P2_TASK_NEW_PROBLEM_DROPPED;
    }
    else if (r == 200)
    {
        /* 200 - the problem was accepted */
        *new_path = xstrdup(abrt_p2_object_path(task->pv->p2tnp_obj));

        code = ABRT_P2_TASK_NEW_PROBLEM_ACCEPTED;

        abrt_p2_entry_set_state(entry, ABRT_P2_ENTRY_STATE_COMPLETE);

        log_debug("Task '%p': New problem '%s'", task, *new_path);

        /* TODO: what about to teach the service to understand task's signals? */
        abrt_p2_service_notify_entry_object(task->pv->p2tnp_service,
                                            task->pv->p2tnp_obj,
                                            error);
    }
    else
    {
        log_debug("Task '%p': Problem was invalid", task);

        abrt_p2_entry_delete(entry,
                             /* allow us to delete the temporary dir */0,
                             error);

        abrt_p2_object_destroy(task->pv->p2tnp_obj);
        task->pv->p2tnp_obj = NULL;

        code = ABRT_P2_TASK_NEW_PROBLEM_INVALID_DATA;
    }

    free(message);
    return code;
}

static AbrtP2TaskCode abrt_p2_task_new_problem_run(AbrtP2Task *task, GError **error)
{
    AbrtP2TaskNewProblem *np = ABRT_P2_TASK_NEW_PROBLEM(task);

    /* Create the temporary entry. If you ask the question how it is possible
     * that the object already exist, then the answer is that if user requested
     * to stop the task after creation of the object. */
    if (np->pv->p2tnp_obj == NULL)
    {
        AbrtP2Object *obj = abrt_p2_task_new_problem_create_directory_task(np, error);
        if (obj == NULL)
            return ABRT_P2_TASK_CODE_ERROR;

        const char *temporary_entry_path = abrt_p2_object_path(obj);
        GVariant *detail_path = g_variant_new_string(temporary_entry_path);
        abrt_p2_task_add_detail(task, "NewProblem.TemporaryEntry", detail_path);

        log_debug("Created temporary entry '%s' for task '%p'",
                  temporary_entry_path,
                  task);

        np->pv->p2tnp_obj = obj;

        if (abrt_p2_task_is_cancelled(task))
            return ABRT_P2_TASK_CODE_CANCELLED;

        if (np->pv->p2tnp_wait_before_notify)
        {
            /* Stop the task to allow users to modify the temporary object */
            log_debug("Stopping NewProblem task '%p'", task);

            return ABRT_P2_TASK_CODE_STOP;
        }
    }

    char *new_path = NULL;
    gint32 code = abrt_p2_task_new_problem_notify_directory_task(np,
                                                                 &new_path,
                                                                 error);
    if (code < 0)
        return ABRT_P2_TASK_CODE_ERROR;

    GVariantDict response;
    g_variant_dict_init(&response, NULL);

    if (new_path != NULL)
    {
        g_variant_dict_insert(&response,
                              "NewProblem.Entry",
                              "s",
                              new_path);
        free(new_path);
    }

    log_debug("NewProblem task '%p' has successfully finished", task);

    abrt_p2_task_set_response(task,
                              g_variant_dict_end(&response));

    return ABRT_P2_TASK_CODE_DONE + code;
}
