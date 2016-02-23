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

  ------------------------------------------------------------------------------

  This file declares functions for org.freedesktop.Problems2.Task interface.

  Base class for tasks.

  Some actions may take too much time and D-Bus connection would simple
  timeout. Thus we need to run those action asynchronously and we have to allow
  users/clients to manage those asynchronous runs.

  This class is rather a wrapper for GTask. A task can have several states.
  It should be possible to stop and start it again. It should also be
  possible to terminate it.

  Offspring can publish details about the task through task 'details' which is
  a string base, key-value structure.
*/
#ifndef ABRT_P2_TASK_H
#define ABRT_P2_TASK_H

#include "libabrt.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

GType abrt_p2_task_get_type (void);

#define ABRT_TYPE_P2_TASK (abrt_p2_task_get_type ())
#define ABRT_P2_TASK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ABRT_TYPE_P2_TASK, AbrtP2Task))
#define ABRT_P2_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ABRT_TYPE_P2_TASK, AbrtP2TaskClass))
#define ABRT_IS_P2_TASK(obj)(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ABRT_TYPE_P2_TASK))
#define ABRT_IS_P2_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ABRT_TYPE_P2_TASK))
#define ABRT_P2_TASK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), ABRT_TYPE_P2_TASK, AbrtP2TaskClass))

typedef struct _AbrtP2Task        AbrtP2Task;
typedef struct _AbrtP2TaskClass   AbrtP2TaskClass;
typedef struct _AbrtP2TaskPrivate AbrtP2TaskPrivate;

static inline void glib_autoptr_cleanup_AbrtP2Task(AbrtP2Task **task)
{
    glib_autoptr_cleanup_GObject((GObject **)task);
}

typedef enum {
    ABRT_P2_TASK_STATUS_NEW,
    ABRT_P2_TASK_STATUS_RUNNING,
    ABRT_P2_TASK_STATUS_STOPPED,
    ABRT_P2_TASK_STATUS_CANCELED,
    ABRT_P2_TASK_STATUS_FAILED,
    ABRT_P2_TASK_STATUS_DONE,
} AbrtP2TaskStatus;

typedef enum {
    ABRT_P2_TASK_CODE_ERROR = -1,
    ABRT_P2_TASK_CODE_STOP,
    ABRT_P2_TASK_CODE_DONE,
    ABRT_P2_TASK_CODE_CANCELLED,
} AbrtP2TaskCode;

struct _AbrtP2TaskClass
{
    GObjectClass parent_class;

    /* Abstract methods */
    AbrtP2TaskCode (* run)(AbrtP2Task *task, GError **error);

    /* Virtual methods */
    void (* start)(AbrtP2Task *task, GVariant *options, GError **error);

    void (* cancel)(AbrtP2Task *task, GError **error);

    void (* finish)(AbrtP2Task *task, GError **error);

    /* Signals */
    void (*status_changed)(AbrtP2Task *task, gint32 status);

    gpointer padding[12];
};

struct _AbrtP2TaskPrivate
{
    gint32 p2t_status;
    GVariant *p2t_details;
    GVariant *p2t_results;
    gint32 p2t_code;
    GCancellable *p2t_cancellable;
};

struct _AbrtP2Task
{
    GObject parent_instance;
    AbrtP2TaskPrivate *pv;
};

AbrtP2TaskStatus abrt_p2_task_status(AbrtP2Task *task);

/* Returns task details in form of key-value entries.
 */
GVariant *abrt_p2_task_details(AbrtP2Task *task);

bool abrt_p2_task_is_cancelled(AbrtP2Task *start);

/* Offspring can provide D-Bus client with information they need to have.
 *
 * For example: a path to the new problem directory of NewProblemTask
 */
void abrt_p2_task_add_detail(AbrtP2Task *task,
            const char *key,
            GVariant *value);

/* Private function for offspring to return their results.
 */
void abrt_p2_task_set_response(AbrtP2Task *task,
            GVariant *response);

void abrt_p2_task_start(AbrtP2Task *start,
            GVariant *options,
            GError **error);

void abrt_p2_task_cancel(AbrtP2Task *start,
            GError **error);

/* Retrieve results of a finished task.
 */
void abrt_p2_task_finish(AbrtP2Task *start,
            GVariant **result,
            gint32 *code,
            GError **error);

/* Runs a task in non-interactive mode.
 *
 * For example, stopped tasks will be automatically canceled.
 */
void abrt_p2_task_autonomous_run(AbrtP2Task *task,
        GError **error);

G_END_DECLS

#endif/*ABRT_P2_TASK_H*/
