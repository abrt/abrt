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

#ifndef ABRT_HTTPS_UTILS_H_
#define ABRT_HTTPS_UTILS_H_

#include <libreport/client.h>
#include "libabrt.h"

#if HAVE_LOCALE_H
#include <locale.h>
#endif

struct language
{
    char *charset;
};
void get_language(struct language *lang);

struct https_cfg
{
    const char *uri;
    bool ssl_allow_insecure;
};

void alert_server_error(const char *peer_name);
void alert_connection_error(const char *peer_name);

#endif
