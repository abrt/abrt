/*
    Plugin.h - header file for plugin. It contains mandatory macros
               and common function for plugins

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

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include <string>
#include <map>

#define PLUGINS_MAGIC_NUMBER 1

#define PLUGINS_CONF_EXTENSION "conf"
#define PLUGINS_LIB_EXTENSION "so"
#define PLUGINS_LIB_PREFIX "lib"

class CPlugin
{
    public:
        virtual ~CPlugin() {}

        virtual void Init() {}
        virtual void DeInit() {}
        virtual void LoadSettings(const std::string& pPath) {}
};

typedef enum { ANALYZER, ACTION, REPORTER, DATABASE } plugin_type_t;
const char* const plugin_type_str_t[] = {"Analyzer", "Action", "Reporter", "Database"};

typedef struct SPluginInfo
{
    const plugin_type_t m_Type;
    const std::string m_sName;
    const std::string m_sVersion;
    const std::string m_sDescription;
    const std::string m_sEmail;
    const std::string m_sWWW;
    const int m_nMagicNumber;
} plugin_info_t;

#define PLUGIN_IFACE extern "C"

#define PLUGIN_INFO(type, plugin_class, name, version, description, email, www)\
    PLUGIN_IFACE CPlugin* plugin_new()\
    {\
        return new plugin_class();\
    }\
    PLUGIN_IFACE const plugin_info_t plugin_info =\
    {\
        type,\
        name,\
        version,\
        description,\
        email,\
        www,\
        PLUGINS_MAGIC_NUMBER,\
    };

#endif /* PLUGIN_H_ */
