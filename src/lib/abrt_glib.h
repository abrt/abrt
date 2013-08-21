/*
    Copyright (C) 2013  ABRT team
    Copyright (C) 2013  RedHat Inc

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
#ifndef _ABRT_GLIB_H_
#define _ABRT_GLIB_H_

#include <glib-2.0/glib.h>

GList *string_list_from_variant(GVariant *variant);

GVariant *variant_from_string_list(const GList *strings);

GIOChannel *abrt_gio_channel_unix_new(int fd);

#endif /*_ABRT_GLIB_H_*/
