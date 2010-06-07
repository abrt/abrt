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

#include "abrt_types.h"
#include "crash_types.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

#define PLUGINS_MAGIC_NUMBER 6

#define PLUGINS_CONF_EXTENSION "conf"
#define PLUGINS_LIB_EXTENSION "so"
#define PLUGINS_LIB_PREFIX "lib"

/**
 * An abstract class. The class defines a common plugin interface. If a plugin
 * has some settings, then a *Settings(*) method has to be written.
 */
class CPlugin
{
    protected:
        map_plugin_settings_t m_pSettings;

    public:
        CPlugin();
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
         * A method, which takes a settings and apply them. It is not a mandatory method.
         * @param pSettings Plugin's settings
         */
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
        /**
         * A method, which return current settings. It is not mandatory method.
         * @return Plugin's settings
         */
        virtual const map_plugin_settings_t& GetSettings();
};

/**
 * An enum of plugin types.
 */
typedef enum {
    ANALYZER,    /**< An analyzer plugin*/
    ACTION,      /**< An action plugin*/
    REPORTER,    /**< A reporter plugin*/
    DATABASE,    /**< A database plugin*/
    MAX_PLUGIN_TYPE = DATABASE,
} plugin_type_t;

/**
 * A struct contains all needed data about particular plugin.
 */
typedef struct SPluginInfo
{
    const plugin_type_t m_Type;         /**< Plugin type.*/
    const char *const m_sName;          /**< Plugin name.*/
    const char *const m_sVersion;       /**< Plugin version.*/
    const char *const m_sDescription;   /**< Plugin description.*/
    const char *const m_sEmail;         /**< Plugin author's email.*/
    const char *const m_sWWW;           /**< Plugin's home page.*/
    const char *const m_sGTKBuilder;    /**< Plugin's gui description.*/
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

/* helper functions */
char* make_description_bz(const map_crash_data_t& pCrashData);
char* make_description_reproduce_comment(const map_crash_data_t& pCrashData);
char* make_description_logger(const map_crash_data_t& pCrashData);

/**
 * Loads settings and stores it in second parameter. On success it
 * returns true, otherwise returns false.
 *
 * @param path A path of config file.
 *  Config file consists of "key=value" lines.
 * @param settings A readed plugin's settings.
 * @param skipKeysWithoutValue
 *  If true, lines in format "key=" (without value) are skipped.
 *  Otherwise empty value "" is inserted into pSettings.
 * @return if it success it returns true, otherwise it returns false.
 */
extern bool LoadPluginSettings(const char *pPath,
			       map_plugin_settings_t& pSettings,
			       bool skipKeysWithoutValue = true);

#endif
