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
#include "Language.h"
#include "Reporter.h"
#include "Database.h"
#include "Application.h"

class CPluginManager
{
	private:
		typedef std::map<std::string, CABRTPlugin*> map_abrt_plugins_t;
		typedef std::map<std::string, CPlugin*> map_plugins_t;


		map_abrt_plugins_t m_mapABRTPlugins;
		map_plugins_t m_mapPlugins;

		std::string m_sPlugisConfDir;
		std::string m_sPlugisLibDir;

	public:
		CPluginManager(const std::string& pPlugisConfDir,
					   const std::string& pPlugisLibDir);

		~CPluginManager();

		void LoadPlugins();
		void UnLoadPlugins();

		void LoadPlugin(const std::string& pName);
		void UnLoadPlugin(const std::string& pName);
        void RegisterPlugin(const std::string& pName);
        void UnRegisterPlugin(const std::string& pName);

		CLanguage* GetLanguage(const std::string& pName);
		CReporter* GetReporter(const std::string& pName);
		CApplication* GetApplication(const std::string& pName);
		CDatabase* GetDatabase(const std::string& pName);

};

#endif /*PLUGINMANAGER_H_*/
