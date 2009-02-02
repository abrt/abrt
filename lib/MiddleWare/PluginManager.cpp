/*
    PluginManager.cpp

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

#include <iostream>
#include "PluginManager.h"
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

CPluginManager::CPluginManager(const std::string& pPlugisConfDir,
							   const std::string& pPlugisLibDir) :
	m_sPlugisConfDir(pPlugisConfDir),
	m_sPlugisLibDir(pPlugisLibDir)
{}

CPluginManager::~CPluginManager()
{
	map_crash_catcher_plugins_t::iterator it_p;
	while ((it_p = m_mapCrashCatcherPlugins.begin()) != m_mapCrashCatcherPlugins.end())
	{
		std::string pluginName = it_p->first;
		UnLoadPlugin(pluginName);
	}
}

void CPluginManager::LoadPlugins()
{
	DIR *dir = opendir(m_sPlugisConfDir.c_str());
	struct dirent *dent = NULL;
	if (dir != NULL)
	{
		while ((dent = readdir(dir)) != NULL)
		{
			if (dent->d_type == DT_REG)
			{
				std::string name = dent->d_name;
				std::string extension = name.substr(name.length()-sizeof(PLUGINS_CONF_EXTENSION)+1);
				if (extension == PLUGINS_CONF_EXTENSION)
				{
					name.erase(name.length()-sizeof(PLUGINS_CONF_EXTENSION));
					LoadPlugin(name);
				}
			}
		}
		closedir(dir);
	}
}

void CPluginManager::LoadPlugin(const std::string& pName)
{
	if (m_mapCrashCatcherPlugins.find(pName) == m_mapCrashCatcherPlugins.end())
	{
		CCrashCatcherPlugin* crashCatcherPlugin = NULL;
		CPlugin* plugin = NULL;
		try
		{
			std::string libPath = m_sPlugisLibDir + "/lib" + pName + "." + PLUGINS_LIB_EXTENSIONS;
			crashCatcherPlugin = new CCrashCatcherPlugin(libPath);
			if (crashCatcherPlugin->GetMagicNumber() != PLUGINS_MAGIC_NUMBER)
			{
				throw std::string("non-compatible plugin");
			}
			crashCatcherPlugin->LoadSettings(m_sPlugisConfDir + "/" + pName + "." + PLUGINS_CONF_EXTENSION);
			std::cerr << "Loaded Plugin " << pName << " (" << crashCatcherPlugin->GetVersion() << ") " << "succesfully loaded." << std::endl;
			std::cerr << "	Description: " << crashCatcherPlugin->GetDescription() << std::endl;
			std::cerr << "	Email: " << crashCatcherPlugin->GetEmail() << std::endl;
			std::cerr << "	WWW: " << crashCatcherPlugin->GetWWW() << std::endl;
			m_mapCrashCatcherPlugins[pName] = crashCatcherPlugin;

			if (crashCatcherPlugin->IsEnabled())
			{
				plugin = crashCatcherPlugin->PluginNew();
				plugin->Init(crashCatcherPlugin->GetSettings());
				RegisterPlugin(plugin, pName, crashCatcherPlugin->GetType());
			}

		}
		catch (std::string sError)
		{
			if (plugin != NULL)
			{
				delete plugin;
			}
			if (crashCatcherPlugin != NULL)
			{
				delete plugin;
			}
			std::cerr << "Failed to load plugin " << pName << " (" << sError << ")." << std::endl;
		}
	}
}

void CPluginManager::UnLoadPlugin(const std::string& pName)
{
	if (m_mapCrashCatcherPlugins.find(pName) != m_mapCrashCatcherPlugins.end())
	{
		UnRegisterPlugin(pName, m_mapCrashCatcherPlugins[pName]->GetType());
		delete m_mapCrashCatcherPlugins[pName];
		m_mapCrashCatcherPlugins.erase(pName);
		std::cerr << "Plugin " << pName << " sucessfully unloaded." << std::endl;
	}
}


void CPluginManager::RegisterPlugin(CPlugin* pPlugin,
		                            const std::string pName,
		                            const plugin_type_t& pPluginType)
{
	switch (pPluginType)
	{
		case LANGUAGE:
			{
				m_mapLanguages[pName] = (CLanguage*)pPlugin;
				std::cerr << "Registred Language plugin " << pName << std::endl;
			}
			break;
		case REPORTER:
			{
				m_mapReporters[pName] = (CReporter*)pPlugin;
				std::cerr << "Registred Reporter plugin " << pName << std::endl;
			}
			break;
		case APPLICATION:
			{
				m_mapApplications[pName] = (CApplication*)pPlugin;
				std::cerr << "Registred Application plugin " << pName << std::endl;
			}
			break;
		case DATABASE:
			{
				m_mapDatabases[pName] = (CDatabase*)pPlugin;
				std::cerr << "Registred Database plugin " << pName << std::endl;
			}
			break;
		default:
			{
				std::cerr << "Trying to register unknown type of plugin." << std::endl;
			}
			break;
	}
}

void CPluginManager::UnRegisterPlugin(const std::string pName, const plugin_type_t& pPluginType)
{
	switch (pPluginType)
	{
		case LANGUAGE:
			{
				if (m_mapLanguages.find(pName) != m_mapLanguages.end())
				{
					m_mapLanguages[pName]->DeInit();
					delete m_mapLanguages[pName];
					m_mapLanguages.erase(pName);
					std::cerr << "UnRegistred Language plugin " << pName << std::endl;
				}
			}
			break;
		case REPORTER:
			{
				if (m_mapReporters.find(pName) != m_mapReporters.end())
				{
					m_mapReporters[pName]->DeInit();
					delete m_mapReporters[pName];
					m_mapReporters.erase(pName);
					std::cerr << "UnRegistred Reporter plugin " << pName << std::endl;
				}
			}
			break;
		case APPLICATION:
			{
				if (m_mapApplications.find(pName) != m_mapApplications.end())
				{
					m_mapApplications[pName]->DeInit();
					delete m_mapApplications[pName];
					m_mapApplications.erase(pName);
					std::cerr << "UnRegistred Application plugin " << pName << std::endl;
				}
			}
			break;
		case DATABASE:
			{
				if (m_mapDatabases.find(pName) != m_mapDatabases.end())
				{
					m_mapDatabases[pName]->DeInit();
					delete m_mapDatabases[pName];
					m_mapDatabases.erase(pName);
					std::cerr << "UnRegistred Database plugin " << pName << std::endl;
				}
			}
			break;
		default:
			std::cerr << "Trying to unregister unknown type of plugin." << std::endl;
			break;
	}
}

CLanguage* CPluginManager::GetLanguage(const std::string& pName)
{
	if (m_mapLanguages.find(pName) != m_mapLanguages.end())
	{
		return m_mapLanguages[pName];
	}

	return NULL;
}

CReporter* CPluginManager::GetReporter(const std::string& pName)
{
	if (m_mapReporters.find(pName) != m_mapReporters.end())
	{
		return m_mapReporters[pName];
	}

	return NULL;
}

CApplication* CPluginManager::GetApplication(const std::string& pName)
{
	if (m_mapApplications.find(pName) != m_mapApplications.end())
	{
		return m_mapApplications[pName];
	}

	return NULL;
}

CDatabase* CPluginManager::GetDatabase(const std::string& pName)
{
	if (m_mapDatabases.find(pName) != m_mapDatabases.end())
	{
		return m_mapDatabases[pName];
	}

	return NULL;
}

