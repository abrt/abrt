/*
  Copyright (C) 2012  ABRT team

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

#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>

#include "libabrt.h"
#include "abrt-polkit.h"

#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

/*number of seconds: timeout for the authorization*/
#define POLKIT_TIMEOUT 20

#ifdef HAVE_POLKIT
static gboolean do_cancel(GCancellable* cancellable)
{
    log_warning("Timer has expired; cancelling authorization check\n");
    g_cancellable_cancel(cancellable);
    return FALSE;
}
#endif

#ifdef HAVE_POLKIT
static PolkitResult do_check(PolkitSubject *subject, const char *action_id)
{
    PolkitAuthority *authority;
    PolkitAuthorizationResult *auth_result;
    PolkitResult result = PolkitNo;
    GError *error = NULL;
    GCancellable * cancellable;

    cancellable = g_cancellable_new();

    /* we ignore the error for now .. */
    authority = polkit_authority_get_sync(cancellable, NULL);

    guint cancel_timeout = g_timeout_add(POLKIT_TIMEOUT * 1000,
                   (GSourceFunc) do_cancel,
                   cancellable);

    auth_result = polkit_authority_check_authorization_sync(authority,
                subject,
                action_id,
                NULL,
                POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                cancellable,
                &error);
    g_object_unref(cancellable);
    g_object_unref(authority);
    g_source_remove(cancel_timeout);
    g_object_unref(subject);
    if (error)
    {
        g_error_free(error);
        return PolkitUnknown;
    }

    if (!auth_result)
        return PolkitUnknown;

    if (polkit_authorization_result_get_is_challenge(auth_result))
    {
        /* Can't happen (happens only with
         * POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE flag) */
        result = PolkitChallenge;
        goto out;
    }

    if (polkit_authorization_result_get_is_authorized(auth_result))
    {
        result = PolkitYes;
        goto out;
    }

out:
    g_object_unref(auth_result);
    return result;
}
#endif

PolkitResult polkit_check_authorization_dname(const char *dbus_name, const char *action_id)
{
#ifdef HAVE_POLKIT
    glib_init();

    PolkitSubject *subject = polkit_system_bus_name_new(dbus_name);
    return do_check(subject, action_id);
#else
    log_warning("Polkit disabled. Everyone has access to private data");
    return PolkitYes;
#endif
}

PolkitResult polkit_check_authorization_pid(pid_t pid, const char *action_id)
{
#ifdef HAVE_POLKIT
    glib_init();

    PolkitSubject *subject = polkit_unix_process_new_for_owner(pid,
            /*use start_time from /proc*/0,
            /*use uid from /proc*/ -1);

    return do_check(subject, action_id);
#else
    log_warning("Polkit disabled. Everyone has access to private data");
    return PolkitYes;
#endif
}
