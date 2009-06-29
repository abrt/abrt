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

#include <map>
#include <string>
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
		typedef std::map<std::string, CABRTPlugin*> map_abrt_plugins_t;
		typedef std::map<std::string, CPlugin*> map_plugins_t;

		/**
		 * Loaded plugins. A key is a plugin name.
		 */
		map_abrt_plugins_t m_mapABRTPlugins;
		/**
		 * Registered plugins. A key is a plugin name.
		 */
		map_plugins_t m_mapPlugins;
		/**
		 * Plugins configuration directory (e.g. /etc/abrt/plugins, ...).
		 */
		std::string m_sPlugisConfDir;
		/**
		 * Plugins library directory (e.g. /usr/lib/abrt/plugins, ...).
		 */
		std::string m_sPlugisLibDir;

	public:
	    /**
	     * A constructor.
	     * @param pPlugisConfDir A plugins configuration directory.
	     * @param pPlugisLibDir A plugins library directory.
	     */
		CPluginManager(const std::string& pPlugisConfDir,
					   const std::string& pPlugisLibDir);
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
		void LoadPlugin(const std::string& pName);
		/**
		 * A method, which unloads particular plugin.
		 * @param pName A plugin name.
		 */
		void UnLoadPlugin(const std::string& pName);
		/**
		 * A method, which registers particular plugin.
		 * @param pName A plugin name.
		 */
        void RegisterPlugin(const std::string& pName);
        /**
         * A method, which unregister particular plugin.
         * @param pName A plugin name.
         */
        void UnRegisterPlugin(const std::string& pName);
        /**
         * A method, which returns instance of particular analyzer plugin.
         * @param pName A plugin name.
         * @return An analyzer plugin.
         */
		CAnalyzer* GetAnalyzer(const std::string& pName);
        /**
         * A method, which returns instance of particular reporter plugin.
         * @param pName A plugin name.
         * @return A reporter plugin.
         */
		CReporter* GetReporter(const std::string& pName);
        /**
         * A method, which returns instance of particular action plugin.
         * @param pName A plugin name.
         * @return An action plugin.
         */
		CAction* GetAction(const std::string& pName);
        /**
         * A method, which returns instance of particular database plugin.
         * @param pName A plugin name.
         * @return A database plugin.
         */
		CDatabase* GetDatabase(const std::string& pName);
		/**
		 * A method, which returns type of particular plugin.
		 * @param pName A plugin name.
		 * @return A plugin type.
		 */
		plugin_type_t GetPluginType(const std::string& pName);
};

#endif /*PLUGINMANAGER_H_*/
