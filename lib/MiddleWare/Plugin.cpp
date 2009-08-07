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
#include <fstream>

/* class CPlugin's virtuals */
CPlugin::~CPlugin() {}
void CPlugin::Init() {}
void CPlugin::DeInit() {}
void CPlugin::LoadSettings(const std::string& pPath) {}
void CPlugin::SetSettings(const map_plugin_settings_t& pSettings) {}
map_plugin_settings_t CPlugin::GetSettings() {return map_plugin_settings_t();}

void plugin_load_settings(const std::string& path, map_plugin_settings_t& settings)
{
    std::ifstream fIn;
    fIn.open(path.c_str());
    if (fIn.is_open())
    {
        std::string line;
        while (!fIn.eof())
        {
            getline(fIn, line);

            int ii;
            bool is_value = false;
            bool valid = false;
            bool in_quote = false;
            std::string key = "";
            std::string value = "";
            for (ii = 0; ii < line.length(); ii++)
            {
                if (line[ii] == '\"')
                {
                    in_quote = in_quote == true ? false : true;
                }
                if (isspace(line[ii]) && !in_quote)
                {
                    continue;
                }
                if (line[ii] == '#' && !in_quote)
                {
                    break;
                }
                else if (line[ii] == '=' && !in_quote)
                {
                    is_value = true;
                }
                else if (line[ii] == '=' && is_value && !in_quote)
                {
                    key = "";
                    value = "";
                    break;
                }
                else if (!is_value)
                {
                    key += line[ii];
                }
                else
                {
                    valid = true;
                    value += line[ii];
                }
            }
            if (valid && !in_quote)
            {
                settings[key] = value;
            }
        }
        fIn.close();
    }
}
