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
#include "Plugin.h"
#include "abrtlib.h"

CPlugin::CPlugin() {}

/* class CPlugin's virtuals */
CPlugin::~CPlugin() {}
void CPlugin::Init() {}
void CPlugin::DeInit() {}
void CPlugin::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;
}

const map_plugin_settings_t& CPlugin::GetSettings()
{
    return m_pSettings;
}

bool LoadPluginSettings(const char *pPath, map_plugin_settings_t& pSettings,
			bool skipKeysWithoutValue /*= true*/)
{
    FILE *fp = fopen(pPath, "r");
    if (!fp)
        return false;

    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        strchrnul(line, '\n')[0] = '\0';
        unsigned ii;
        bool is_value = false;
        bool valid = false;
        bool in_quote = false;
	std::string key;
	std::string value;
        for (ii = 0; line[ii] != '\0'; ii++)
        {
            if (line[ii] == '"')
            {
                in_quote = !in_quote;
            }
            if (isspace(line[ii]) && !in_quote)
            {
                continue;
            }
            if (line[ii] == '#' && !in_quote && key == "")
            {
                break;
            }
            if (line[ii] == '=' && !in_quote)
            {
                is_value = true;
                valid = true;
                continue;
            }
            if (!is_value)
            {
                key += line[ii];
            }
            else
            {
                value += line[ii];
            }
        }

	/* Skip broken or empty lines. */
	if (!valid)
	  continue;

	/* Skip lines with empty key. */
	if (key.length() == 0)
	  continue;

	if (skipKeysWithoutValue && value.length() == 0)
	  continue;

	/* Skip lines with unclosed quotes. */
        if (in_quote)
	  continue;

	pSettings[key] = value;
    }
    fclose(fp);
    return true;
}
