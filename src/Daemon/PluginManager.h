/*
    PluginManager.h - header file for plugin manager. it takes care about
                      (un)loading plugins

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

#ifndef PLUGINMANAGER_H_
#define PLUGINMANAGER_H_

#include "abrt_types.h"
#include "ABRTPlugin.h"
#include "Plugin.h"
#include "Analyzer.h"
#include "Reporter.h"
#include "Database.h"
#include "Action.h"

/**
 * A class. It takes care of loading, registering and manipulating with
 * plugins. When a plugin is loaded, its library is opened, but no plugin
 * instance is created. It is possible after plugin registration.
 */
class CPluginManager
{
    private:
        typedef std::map<std::string, CLoadedModule*> map_loaded_module_t;
        typedef std::map<std::string, CPlugin*> map_plugin_t;

        /**
         * Loaded plugins. A key is a plugin name.
         */
        map_loaded_module_t m_mapLoadedModules;
        /**
         * Registered plugins. A key is a plugin name.
         */
        map_plugin_t m_mapPlugins;

    public:
        /**
         * A constructor.
         * @param pPluginsConfDir A plugins configuration directory.
         * @param pPluginsLibDir A plugins library directory.
         */
        CPluginManager();
        /**
         * A destructor.
         */
        ~CPluginManager();
        /**
         * A method, which loads all plugins in plugins library direcotry.
         */
        void LoadPlugins();
        /**
         * A method, which unregister and unload all loaded plugins.
         */
        void UnLoadPlugins();
        /**
         * A method, which loads particular plugin.
         * @param pName A plugin name.
         */
        CPlugin* LoadPlugin(const char *pName, bool enabled_only = false);
        /**
         * A method, which unloads particular plugin.
         * @param pName A plugin name.
         */
        void UnLoadPlugin(const char *pName);
#ifdef PLUGIN_DYNAMIC_LOAD_UNLOAD
        /**
         * A method, which registers particular plugin.
         * @param pName A plugin name.
         */
        void RegisterPluginDBUS(const char *pName, const char *pDBUSSender);
        /**
         * A method, which unregister particular plugin,
         * called via DBUS
         * @param pName A plugin name.
         * @param pDBUSSender DBUS user identification
         */
        void UnRegisterPluginDBUS(const char *pName, const char *pDBUSSender);
#endif
        /**
         * A method, which returns instance of particular analyzer plugin.
         * @param pName A plugin name.
         * @return An analyzer plugin.
         */
        CAnalyzer* GetAnalyzer(const char *pName);
        /**
         * A method, which returns instance of particular reporter plugin.
         * @param pName A plugin name.
         * @return A reporter plugin.
         */
        CReporter* GetReporter(const char *pName);
        /**
         * A method, which returns instance of particular action plugin.
         * @param pName A plugin name.
         * @return An action plugin.
         */
        CAction* GetAction(const char *pName, bool silent = false);
        /**
         * A method, which returns instance of particular database plugin.
         * @param pName A plugin name.
         * @return A database plugin.
         */
        CDatabase* GetDatabase(const char *pName);
        /**
         * A method, which returns type of particular plugin.
         * @param pName A plugin name.
         * @return A plugin type.
         */
        plugin_type_t GetPluginType(const char *pName);
        /**
         * A method, which gets all plugins info (even those plugins which are
         * disabled). It can be sent via DBus to GUI and displayed to an user.
         * Then user can fill all needed informations like URLs etc.
         * @return A vector of maps <key, value>
         */
        vector_map_string_t GetPluginsInfo();
        /**
         * A method, which sets up a plugin. The settings are also saved in home
         * directory of an user.
         * @param pName A plugin name.
         * @param pUID An uid of user.
         * @param pSettings A plugin's settings.
         */
        void SetPluginSettings(const char *pName,
                               const char *pUID,
                               const map_plugin_settings_t& pSettings);
        /**
         * A method, which returns plugin's settings according to user.
         * @param pName A plugin name.
         * @param pUID An uid of user.
         * @return Plugin's settings accorting to user.
         */
        map_plugin_settings_t GetPluginSettings(const char *pName,
                                                const char *pUID);
};

/**
 * Loads settings and stores it in second parameter. On success it
 * returns true, otherwise returns false.
 * @param path A path of config file.
 * @param settings A readed plugin's settings.
 * @return if it success it returns true, otherwise it returns false.
 */
bool LoadPluginSettings(const char *pPath,
                        map_plugin_settings_t& pSettings);
#endif /*PLUGINMANAGER_H_*/
