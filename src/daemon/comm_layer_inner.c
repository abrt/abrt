/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "abrtlib.h"
#include "CommLayerServerDBus.h"
#include "comm_layer_inner.h"

static char *client_name = NULL;

/* called via [p]error_msg() */
static void warn_client(const char *msg)
{
    const char* peer = client_name;
    if (peer)
    {
        send_dbus_sig_Warning(msg, peer);
    }
}

void init_daemon_logging(void)
{
    g_custom_logger = &warn_client;
}

void set_client_name(const char *name)
{
    free(client_name);
    client_name = xstrdup(name);
}

void update_client(const char *fmt, ...)
{
    const char* peer = client_name;
    if (!peer)
        return;

    va_list p;
    va_start(p, fmt);
    char *msg = xvasprintf(fmt, p);
    va_end(p);

    VERB1 log("Update('%s'): %s", peer, msg);
    send_dbus_sig_Update(msg, peer);

    free(msg);
}
