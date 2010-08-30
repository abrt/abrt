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

// caller is reposible for freeing *product* and *version*
void parse_release(const char *release, char** product, char** version)
{
    if (strstr(release, "Rawhide"))
    {
        *product = xstrdup("Fedora");
        *version = xstrdup("rawhide");
        VERB3 log("%s: version:'%s' product:'%s'", __func__, *version, *product);
        return;
    }

    struct strbuf *buf_product = strbuf_new();
    if (strstr(release, "Fedora"))
        strbuf_append_str(buf_product, "Fedora");
    else if (strstr(release, "Red Hat Enterprise Linux"))
        strbuf_append_str(buf_product, "Red Hat Enterprise Linux ");

    const char *r = strstr(release, "release");
    const char *space = r ? strchr(r, ' ') : NULL;

    struct strbuf *buf_version = strbuf_new();
    if (space++)
    {
        while (*space != '\0' && *space != ' ')
        {
            /* Eat string like "5.2" */
            strbuf_append_char(buf_version, *space);
            if ((strcmp(buf_product->buf, "Red Hat Enterprise Linux ") == 0))
                strbuf_append_char(buf_product, *space);

            space++;
        }
    }

    *version = strbuf_free_nobuf(buf_version);
    *product = strbuf_free_nobuf(buf_product);

    VERB3 log("%s: version:'%s' product:'%s'", __func__, *version, *product);
}
