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
#include "ABRTException.h"
#include "CommLayerInner.h"
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

CPluginManager::CPluginManager(const std::string& pPlugisConfDir,
							   const std::string& pPlugisLibDir) :
    m_sPlugisConfDir(pPlugisConfDir),
    m_sPlugisLibDir(pPlugisLibDir)
{}

CPluginManager::~CPluginManager()
{}

void CPluginManager::LoadPlugins()
{
    DIR *dir = opendir(m_sPlugisLibDir.c_str());
    struct dirent *dent = NULL;
    if (dir != NULL)
    {
        while ((dent = readdir(dir)) != NULL)
        {
            if (dent->d_type == DT_REG)
            {
                std::string name = dent->d_name;
                std::string extension = name.substr(name.length()-sizeof(PLUGINS_LIB_EXTENSION)+1);
                if (extension == PLUGINS_LIB_EXTENSION)
                {
                    name.erase(0, sizeof(PLUGINS_LIB_PREFIX) - 1);
                    name.erase(name.length() - sizeof(PLUGINS_LIB_EXTENSION));
                    LoadPlugin(name);
                }
            }
        }
        closedir(dir);
    }
}

void CPluginManager::UnLoadPlugins()
{
    map_abrt_plugins_t::iterator it_p;
      while ((it_p = m_mapABRTPlugins.begin()) != m_mapABRTPlugins.end())
      {
          std::string pluginName = it_p->first;
          UnLoadPlugin(pluginName);
      }
}

void CPluginManager::LoadPlugin(const std::string& pName)
{
    if (m_mapABRTPlugins.find(pName) == m_mapABRTPlugins.end())
    {
        CABRTPlugin* abrtPlugin = NULL;
        try
        {
            std::string libPath = m_sPlugisLibDir + "/" + PLUGINS_LIB_PREFIX + pName + "." + PLUGINS_LIB_EXTENSION;
            abrtPlugin = new CABRTPlugin(libPath);
            if (abrtPlugin->GetMagicNumber() != PLUGINS_MAGIC_NUMBER ||
                (abrtPlugin->GetType() < ANALYZER && abrtPlugin->GetType() > DATABASE))
            {
                throw CABRTException(EXCEP_PLUGIN, "CPluginManager::LoadPlugin(): non-compatible plugin");
            }
            comm_layer_inner_debug("Plugin " + pName + " (" + abrtPlugin->GetVersion() + ") succesfully loaded.");
            m_mapABRTPlugins[pName] = abrtPlugin;
        }
        catch (CABRTException& e)
        {
            if (abrtPlugin != NULL)
            {
                delete abrtPlugin;
            }
            comm_layer_inner_warning("CPluginManager::LoadPlugin(): " + e.what());
            comm_layer_inner_warning("Failed to load plugin " + pName);
        }
    }
}

void CPluginManager::UnLoadPlugin(const std::string& pName)
{
    if (m_mapABRTPlugins.find(pName) != m_mapABRTPlugins.end())
    {
        UnRegisterPlugin(pName);
        delete m_mapABRTPlugins[pName];
        m_mapABRTPlugins.erase(pName);
        comm_layer_inner_debug("Plugin " + pName + " sucessfully unloaded.");
    }
}


void CPluginManager::RegisterPlugin(const std::string& pName)
{
    if (m_mapABRTPlugins.find(pName) != m_mapABRTPlugins.end())
    {
        if (m_mapPlugins.find(pName) == m_mapPlugins.end())
        {
            std::string path = m_sPlugisConfDir + "/" + pName + "." + PLUGINS_CONF_EXTENSION;
            CPlugin* plugin = m_mapABRTPlugins[pName]->PluginNew();
            try
            {
                plugin->Init();
                plugin->LoadSettings(path);
            }
            catch (std::string sError)
            {
                comm_layer_inner_warning("Can not initialize plugin " + pName + "("
                                        + std::string(plugin_type_str_t[m_mapABRTPlugins[pName]->GetType()])
                                        + ")");
                UnLoadPlugin(pName);
                return;
            }
            m_mapPlugins[pName] = plugin;
            comm_layer_inner_debug("Registred plugin " + pName + "("
                                  + std::string(plugin_type_str_t[m_mapABRTPlugins[pName]->GetType()])
                                  + ")");
        }
    }
}

void CPluginManager::UnRegisterPlugin(const std::string& pName)
{
    if (m_mapABRTPlugins.find(pName) != m_mapABRTPlugins.end())
    {
        if (m_mapPlugins.find(pName) != m_mapPlugins.end())
        {
            m_mapPlugins[pName]->DeInit();
            delete m_mapPlugins[pName];
            m_mapPlugins.erase(pName);
            comm_layer_inner_debug("UnRegistred plugin " + pName + "("
                                  + std::string(plugin_type_str_t[m_mapABRTPlugins[pName]->GetType()])
                                  + ")");
        }
    }
}

CAnalyzer* CPluginManager::GetAnalyzer(const std::string& pName)
{
    if (m_mapPlugins.find(pName) == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAnalyzer():"
                                            "Analyzer plugin: '"+pName+"' is not loaded.");
    }
    return dynamic_cast<CAnalyzer*>(m_mapPlugins[pName]);
}

CReporter* CPluginManager::GetReporter(const std::string& pName)
{
    if (m_mapPlugins.find(pName) == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetReporter():"
                                           "Reporter plugin: '"+pName+"' is not loaded.");
    }
    return dynamic_cast<CReporter*>(m_mapPlugins[pName]);
}

CAction* CPluginManager::GetAction(const std::string& pName)
{
    if (m_mapPlugins.find(pName) == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAction():"
                                           "Action plugin: '"+pName+"' is not loaded.");
    }
    return dynamic_cast<CAction*>(m_mapPlugins[pName]);
}

CDatabase* CPluginManager::GetDatabase(const std::string& pName)
{
    if (m_mapPlugins.find(pName) == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetDatabase():"
                                           "Database plugin: '"+pName+"' is not loaded.");
    }
    return dynamic_cast<CDatabase*>(m_mapPlugins[pName]);
}

