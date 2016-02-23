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

  This file declares functions for org.freedesktop.Problems2.Session interface.

  Every client must have a session, therefore sessions are created
  automatically. Every session belongs to a one D-Bus caller (client) and no
  other D-Bus caller can access it.

  Session hold status of authorization and manages client's tasks. Client's
  session destroys client's task when client disconnects. Session should not
  allow a client to consume too much resources by allowing him to create
  enormous number of tasks. D-Bus clients must not be able to work with task of
  other clients.

  Session have two public states and one internal state. The public states are
  authorized or not. The internal state is 'authorization pending' - when
  client requested to authorize the session but PolKit hasn't replied yet.

  If requested, authorization should be automatically granted to users who
  already owns an authorized session.
*/

#ifndef ABRT_PROBLEMS2_SESSION_H
#define ABRT_PROBLEMS2_SESSION_H

#include "abrt_problems2_task.h"

#include <polkit/polkit.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <inttypes.h>

G_BEGIN_DECLS

#define TYPE_ABRT_P2_SESSION abrt_p2_session_get_type ()
GType abrt_p2_session_get_type (void);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
typedef struct _AbrtP2Session AbrtP2Session;

typedef struct
{
    GObjectClass parent_class;

    void (*authorization_changed)(AbrtP2Session *session, gint32 status);
} AbrtP2SessionClass;

_GLIB_DEFINE_AUTOPTR_CHAINUP(AbrtP2Session, GObject)

static inline AbrtP2Session *ABRT_P2_SESSION(gconstpointer ptr)
{
    return G_TYPE_CHECK_INSTANCE_CAST(ptr, abrt_p2_session_get_type(), AbrtP2Session);
}

static inline gboolean ABRT_P2_IS_SESSION(gconstpointer ptr)
{
    return G_TYPE_CHECK_INSTANCE_TYPE(ptr, abrt_p2_session_get_type());
}
G_GNUC_END_IGNORE_DEPRECATIONS


AbrtP2Session *abrt_p2_session_new(char *caller, uid_t uid);

uid_t abrt_p2_session_uid(AbrtP2Session *session);

const char *abrt_p2_session_caller(AbrtP2Session *session);

int abrt_p2_session_is_authorized(AbrtP2Session *session);

typedef enum {
    ABRT_P2_SESSION_AUTHORIZE_FAILED = -1,
    ABRT_P2_SESSION_AUTHORIZE_GRANTED = 0,
    ABRT_P2_SESSION_AUTHORIZE_ACCEPTED = 1,
    ABRT_P2_SESSION_AUTHORIZE_PENDING = 2,
} AbrtP2SessionAuthRequestRet;

typedef enum {
    ABRT_P2_SESSION_CHANGED_AUTHORIZED = 0,
    ABRT_P2_SESSION_CHANGED_PENDING = 1,
    ABRT_P2_SESSION_CHANGED_NOT_AUTHORIZED = 2,
    ABRT_P2_SESSION_CHANGED_FAILED = 3,
} AbrtP2SessionAuthChangedStatus;

AbrtP2SessionAuthRequestRet abrt_p2_session_authorize(AbrtP2Session *session,
            GVariant *parameters);

AbrtP2SessionAuthRequestRet abrt_p2_session_grant_authorization(AbrtP2Session *session);

void abrt_p2_session_close(AbrtP2Session *session);

int abrt_p2_session_check_sanity(AbrtP2Session *session,
            const char *caller,
            uid_t caller_uid,
            GError **error);

uint32_t abrt_p2_session_add_task(AbrtP2Session *session,
            AbrtP2Task *task,
            GError **error);

void abrt_p2_session_remove_task(AbrtP2Session *session,
            AbrtP2Task *task,
            GError **error);

int abrt_p2_session_owns_task(AbrtP2Session *session,
            AbrtP2Task *task);

GList *abrt_p2_session_tasks(AbrtP2Session *session);

int abrt_p2_session_tasks_count(AbrtP2Session *session);

void abrt_p2_session_clean_tasks(AbrtP2Session *session);

/*
 * Shared PolKit authority with other entities.
 */
PolkitAuthority *abrt_p2_session_class_set_polkit_authority(PolkitAuthority *pk_authority);

PolkitAuthority *abrt_p2_session_class_polkit_authority(void);

PolkitAuthority *abrt_p2_session_class_release_polkit_authority(void);

G_END_DECLS

#endif/*ABRT_PROBLEMS2_SESSION_H*/
