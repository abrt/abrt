/*
    Polkit.cpp - PolicyKit integration for ABRT

    Copyright (C) 2009  Daniel Novotny (dnovotny@redhat.com)
    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <polkit/polkit.h>
#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>

#include "Polkit.h"
#include "abrtlib.h"

/*number of seconds: timeout for the authorization*/
#define POLKIT_TIMEOUT 20

static gboolean do_cancel(GCancellable* cancellable)
{
    log("Timer has expired; cancelling authorization check\n");
    g_cancellable_cancel(cancellable);
    return FALSE;
}


static PolkitResult do_check(PolkitSubject *subject, const char *action_id)
{
    PolkitAuthority *authority;
    PolkitAuthorizationResult *result;
    GError *error = NULL;
    GCancellable * cancellable;

    authority = polkit_authority_get();
    cancellable = g_cancellable_new();

    guint cancel_timeout = g_timeout_add(POLKIT_TIMEOUT * 1000,
                   (GSourceFunc) do_cancel,
                   cancellable);

    result = polkit_authority_check_authorization_sync(authority,
                subject,
                action_id,
                NULL,
                POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                cancellable,
                &error);
    g_object_unref(authority);
    g_source_remove(cancel_timeout);
    if (error)
    {
        g_error_free(error);
        return PolkitUnknown;
    }

    if (result)
    {
        if (polkit_authorization_result_get_is_challenge(result))
        {
            /* Can't happen (happens only with
             * POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE flag) */
            g_object_unref(result);
            return PolkitChallenge;
        }
        if (polkit_authorization_result_get_is_authorized(result))
        {
            g_object_unref(result);
            return PolkitYes;
        }
        g_object_unref(result);
        return PolkitNo;
    }

    return PolkitUnknown;
}

PolkitResult polkit_check_authorization(const char *dbus_name, const char *action_id)
{
    g_type_init();
    PolkitSubject *subject = polkit_system_bus_name_new(dbus_name);
    return do_check(subject, action_id);
}

PolkitResult polkit_check_authorization(pid_t pid, const char *action_id)
{
    g_type_init();
    PolkitSubject *subject = polkit_unix_process_new(pid);
    return do_check(subject, action_id);
}
