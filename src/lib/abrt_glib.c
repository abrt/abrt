/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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
#include <glib-2.0/glib.h>
#include "libabrt.h"

GList *string_list_from_variant(GVariant *variant)
{
    GList *list = NULL;
    GVariantIter *iter;
    gchar *str;
    g_variant_get(variant, "as", &iter);
    while (g_variant_iter_loop(iter, "s", &str))
    {
        VERB1 log("adding: %s", str);
        list = g_list_prepend(list, xstrdup(str));
    }
    g_variant_unref(variant);

    /* we were prepending items, so we should reverse the list to not confuse people
     * by returning items in reversed order than it's in the variant
    */
    return g_list_reverse(list);
}

GVariant *variant_from_string_list(GList *strings)
{
    GVariantBuilder *builder;
    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    while (strings)
    {
        g_variant_builder_add(builder, "s", (char*)strings->data);
        free(strings->data);
        strings = g_list_delete_link(strings, strings);
    }

    GVariant *variant = g_variant_new("(as)", builder);
    g_variant_builder_unref(builder);

    return variant;
}