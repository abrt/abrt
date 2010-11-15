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

void parse_args(const char *psArgs, vector_string_t& pArgs, int quote)
{
    unsigned ii;
    bool inside_quotes = false;
    std::string item;

    for (ii = 0; psArgs[ii]; ii++)
    {
        if (quote != -1)
        {
            if (psArgs[ii] == quote)
            {
                inside_quotes = !inside_quotes;
                continue;
            }
            /* inside quotes we support escaping with \x */
            if (inside_quotes && psArgs[ii] == '\\' && psArgs[ii+1])
            {
                ii++;
                item += psArgs[ii];
                continue;
            }
        }
        if (psArgs[ii] == ',' && !inside_quotes)
        {
            pArgs.push_back(item);
            item.clear();
            continue;
        }
        item += psArgs[ii];
    }

    if (item.size() != 0)
    {
        pArgs.push_back(item);
    }
}
