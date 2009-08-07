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

#define PLUGINS_MAGIC_NUMBER 4

#define PLUGINS_CONF_EXTENSION "conf"
#define PLUGINS_LIB_EXTENSION "so"
#define PLUGINS_LIB_PREFIX "lib"

typedef std::map<std::string, std::string> map_plugin_settings_t;

/**
 * An abstract class. The class defines a common plugin interface. If a plugin
 * has some settings, then a *Settings(*) method has to be written.
 */
class CPlugin
{
    public:
        /**
         * A destructor.
         */
        virtual ~CPlugin();
        /**
         * A method, which initializes a plugin. It is not mandatory method.
         */
        virtual void Init();
        /**
         * A method, which deinitializes a plugin. It is not mandatory method.
         */
        virtual void DeInit();
        /**
         * A method, which loads a plugin settings from a file. It is not mandatory method.
         * @param pPath A path to plugin configuration file.
         */
        virtual void LoadSettings(const std::string& pPath);
        /**
         * A method, which takes a settings and apply them. It is not a mandatory method.
         * @param pSettings Plugin's settings
         */
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        /**
         * A method, which return current settings. It is not mandatory method.
         * @return Plugin's settings
         */
        virtual map_plugin_settings_t GetSettings();
};

/**
 * An enum of plugin types.
 */
typedef enum {
    ANALYZER,    /**< An analyzer plugin*/
    ACTION,      /**< An action plugin*/
    REPORTER,    /**< A reporter plugin*/
    DATABASE     /**< A database plugin*/
} plugin_type_t;

/**
 * A struct contains all needed data about particular plugin.
 */
typedef struct SPluginInfo
{
    const plugin_type_t m_Type;         /**< Plugin type.*/
    const std::string m_sName;          /**< Plugin name.*/
    const std::string m_sVersion;       /**< Plugin version.*/
    const std::string m_sDescription;   /**< Plugin description.*/
    const std::string m_sEmail;         /**< Plugin author's email.*/
    const std::string m_sWWW;           /**< Plugin's home page.*/
    const std::string m_sGTKBuilder;    /**< Plugin's gui description.*/
    const int m_nMagicNumber;           /**< Plugin magical number.*/
} plugin_info_t;

#define PLUGIN_INFO(type, plugin_class, name, version, description, email, www, gtk_builder)\
    extern "C" CPlugin* plugin_new()\
    {\
        return new plugin_class();\
    }\
    extern "C" const plugin_info_t plugin_info =\
    {\
        type,\
        name,\
        version,\
        description,\
        email,\
        www,\
        gtk_builder,\
        PLUGINS_MAGIC_NUMBER,\
    };

void plugin_load_settings(const std::string& path, map_plugin_settings_t& settings);

#endif /* PLUGIN_H_ */
