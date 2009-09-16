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

static PolkitResult do_check(PolkitSubject *subject, const char *action_id)
{
    PolkitAuthority *authority;
    PolkitAuthorizationResult *result;
    GError *error = NULL;

    authority = polkit_authority_get();

    result = polkit_authority_check_authorization_sync(authority,
                subject,
                action_id,
                NULL,
                POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                NULL,
                &error);

    if (error)
    {
        g_error_free(error);
        return PolkitUnknown;
    }

    if (result)
    {
        if (polkit_authorization_result_get_is_challenge(result))
            /* Can't happen (happens only with
             * POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE flag) */
            return PolkitChallenge;
        if (polkit_authorization_result_get_is_authorized(result))
            return PolkitYes;
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
