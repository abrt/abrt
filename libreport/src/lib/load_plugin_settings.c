/*
    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "libreport.h"

/* Returns false if open failed.
 * Returns empty hash if conf file is empty.
 * TODO: better error detection?
 */
bool load_conf_file(const char *pPath, map_string_h *settings, bool skipKeysWithoutValue)
{
    FILE *fp = stdin;
    if (strcmp(pPath, "-") != 0)
    {
        fp = fopen(pPath, "r");
        if (!fp)
            return false;
    }

    char *line;
    while ((line = xmalloc_fgetline(fp)) != NULL)
    {
        bool in_quote = false;
        /* We are reusing line buffer to form temporary
         * "key\0value\0..." in its beginning
         */
        char *value = NULL;
        char *src;
        char *dst;
        for (src = dst = line; *src; src++)
        {
            char c = *src;
            if (c == '"')
            {
                in_quote = !in_quote;
            }
            if (!in_quote)
            {
                if (isspace(c))
                {
                    continue;
                }
                if (c == '#' && dst == line)
                {
                    break;
                }
                if (c == '=')
                {
                    *dst++ = '\0'; /* terminate key */
                    value = dst; /* remember where value starts */
                    continue;
                }
            }
            *dst++ = c; /* store next key or value char */
        }
	*dst = '\0'; /* terminate value */

        /* Skip broken or empty lines. */
        if (!value)
            goto free_line;

        /* Skip lines with empty key. */
        if (line[0] == '\0')
            goto free_line;

        if (skipKeysWithoutValue && value[0] == '\0')
            goto free_line;

        /* Skip lines with unclosed quotes. */
        if (in_quote)
            goto free_line;

        g_hash_table_replace(settings, xstrdup(line), xstrdup(value));
 free_line:
        free(line);
    }

    if (fp != stdin)
        fclose(fp);

    return true;
}
