/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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

#include "https-utils.h"

void get_language(struct language *lang)
{
    /*
     * Note: ->accept_language and ->accept_charset will always be non-NULL:
     * if we don't know them, they'll be ""; otherwise,
     * they will be fully formed HTTP headers, with \r\n at the end.
     * IOW: they are formatted for adding them to HTTP headers as-is.
     */

    char *locale = setlocale(LC_ALL, NULL);
    if (!locale)
    {
        lang->charset = libreport_xzalloc(1);
        return;
    }

    char *encoding = strchr(locale, '.');
    if (!encoding)
    {
        lang->charset = libreport_xzalloc(1);
        return;
    }

    *encoding = '\0';
    ++encoding;
    lang->charset = g_strdup(encoding);
}

void alert_server_error(const char *peer_name)
{
    if (!peer_name)
        libreport_alert(_("An error occurred on the server side."));
    else
    {
        char *msg = g_strdup_printf(_("A server-side error occurred on '%s'"), peer_name);
        libreport_alert(msg);
        free(msg);
    }
}

void alert_connection_error(const char *peer_name)
{
    if (!peer_name)
        libreport_alert(_("An error occurred while connecting to the server"));
    else
    {
        char *msg = g_strdup_printf(_("An error occurred while connecting to '%s'"), peer_name);
        libreport_alert(msg);
        free(msg);
    }
}
