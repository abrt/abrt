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

#include <fstream>
#include <iostream>
#include "abrtlib.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "Polkit.h"
#include "PluginManager.h"

/**
 * Text representation of plugin types.
 */
static const char* const plugin_type_str[] = {
    "Analyzer",
    "Action",
    "Reporter",
    "Database"
};


bool LoadPluginSettings(const std::string& pPath, map_plugin_settings_t& pSettings)
{
    std::ifstream fIn;
    fIn.open(pPath.c_str());
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
                if (line[ii] == '#' && !in_quote && key == "")
                {
                    break;
                }
                if (line[ii] == '=' && !in_quote)
                {
                    is_value = true;
                    continue;
                }
                if (!is_value)
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
                pSettings[key] = value;
            }
        }
        fIn.close();
        return true;
    }
    return false;
}

/**
 * A function. It saves settings. On success it returns true, otherwise returns false.
 * @param path A path of config file.
 * @param settings Plugin's settings.
 * @return if it success it returns true, otherwise it returns false.
 */
static bool SavePluginSettings(const std::string& pPath, const map_plugin_settings_t& pSettings)
{
    FILE* fOut = fopen(pPath.c_str(), "w");
    if (fOut)
    {
        fprintf(fOut, "# Settings were written by abrt\n");
        map_plugin_settings_t::const_iterator it = pSettings.begin();
        for (; it != pSettings.end(); it++)
        {
            fprintf(fOut, "%s = %s\n", it->first.c_str(), it->second.c_str());
        }
        fclose(fOut);
        return true;
    }
    return false;
}


CPluginManager::CPluginManager()
{}

CPluginManager::~CPluginManager()
{}

void CPluginManager::LoadPlugins()
{
    DIR *dir = opendir(PLUGINS_LIB_DIR);
    if (dir != NULL)
    {
        struct dirent *dent;
        while ((dent = readdir(dir)) != NULL)
        {
            // FIXME: need to handle DT_UNKNOWN too
            if (dent->d_type == DT_REG)
            {
                std::string name = dent->d_name;
                std::string extension = name.substr(name.length() - sizeof(PLUGINS_LIB_EXTENSION) + 1);
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
            std::string libPath = PLUGINS_LIB_DIR"/"PLUGINS_LIB_PREFIX + pName + "."PLUGINS_LIB_EXTENSION;
            abrtPlugin = new CABRTPlugin(libPath.c_str());
            if (abrtPlugin->GetMagicNumber() != PLUGINS_MAGIC_NUMBER ||
                (abrtPlugin->GetType() < ANALYZER && abrtPlugin->GetType() > DATABASE))
            {
                throw CABRTException(EXCEP_PLUGIN, "CPluginManager::LoadPlugin(): non-compatible plugin");
            }
            log("Plugin %s (%s) succesfully loaded", pName.c_str(), abrtPlugin->GetVersion());
            m_mapABRTPlugins[pName] = abrtPlugin;
        }
        catch (CABRTException& e)
        {
            delete abrtPlugin;
            warn_client("CPluginManager::LoadPlugin(): " + e.what());
            warn_client("Failed to load plugin " + pName);
        }
    }
}

void CPluginManager::UnLoadPlugin(const std::string& pName)
{
    map_abrt_plugins_t::iterator abrt_plugin = m_mapABRTPlugins.find(pName);
    if (abrt_plugin != m_mapABRTPlugins.end())
    {
        UnRegisterPlugin(pName);
        delete abrt_plugin->second;
        m_mapABRTPlugins.erase(abrt_plugin);
        log("Plugin %s successfully unloaded", pName.c_str());
    }
}

void CPluginManager::RegisterPlugin(const std::string& pName)
{
    map_abrt_plugins_t::iterator abrt_plugin = m_mapABRTPlugins.find(pName);
    if (abrt_plugin != m_mapABRTPlugins.end())
    {
        if (m_mapPlugins.find(pName) == m_mapPlugins.end())
        {
            CPlugin* plugin = abrt_plugin->second->PluginNew();
            map_plugin_settings_t pluginSettings;

            LoadPluginSettings(PLUGINS_CONF_DIR"/" + pName + "."PLUGINS_CONF_EXTENSION, pluginSettings);
            try
            {
                plugin->Init();
                plugin->SetSettings(pluginSettings);
            }
            catch (CABRTException& e)
            {
                log("Can't initialize plugin %s(%s): %s",
                        pName.c_str(),
                        plugin_type_str[abrt_plugin->second->GetType()],
                        e.what().c_str()
                );
                UnLoadPlugin(pName);
                return;
            }
            m_mapPlugins[pName] = plugin;
            log("Registered plugin %s(%s)", pName.c_str(), plugin_type_str[abrt_plugin->second->GetType()]);
        }
    }
}

void CPluginManager::RegisterPluginDBUS(const std::string& pName,
                     const char * pDBUSSender)
{
    int polkit_result = polkit_check_authorization(pDBUSSender,
                           "org.fedoraproject.abrt.change-daemon-settings");
    if (polkit_result == PolkitYes)
    {
        RegisterPlugin(pName);
    } else
    {
        log("User %s not authorized, returned %d", pDBUSSender, polkit_result);
    }
}

void CPluginManager::UnRegisterPlugin(const std::string& pName)
{
    map_abrt_plugins_t::iterator abrt_plugin = m_mapABRTPlugins.find(pName);
    if (abrt_plugin != m_mapABRTPlugins.end())
    {
        map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
        if (plugin != m_mapPlugins.end())
        {
            plugin->second->DeInit();
            delete plugin->second;
            m_mapPlugins.erase(plugin);
            log("UnRegistered plugin %s(%s)", pName.c_str(), plugin_type_str[abrt_plugin->second->GetType()]);
        }
    }
}

void CPluginManager::UnRegisterPluginDBUS(const std::string& pName,
                     const char * pDBUSSender)
{
    int polkit_result = polkit_check_authorization(pDBUSSender,
                           "org.fedoraproject.abrt.change-daemon-settings");
    if (polkit_result == PolkitYes)
    {
        UnRegisterPlugin(pName);
    } else
    {
        log("user %s not authorized, returned %d", pDBUSSender, polkit_result);
    }
}


CAnalyzer* CPluginManager::GetAnalyzer(const std::string& pName)
{
    map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
    if (plugin == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAnalyzer():"
                                            "Analyzer plugin: '"+pName+"' is not registered.");
    }
    if (m_mapABRTPlugins[pName]->GetType() != ANALYZER)
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAnalyzer():"
                                            "Plugin: '"+pName+"' is not analyzer plugin.");
    }
    return (CAnalyzer*)(plugin->second);
}

CReporter* CPluginManager::GetReporter(const std::string& pName)
{
    map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
    if (plugin == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetReporter():"
                                           "Reporter plugin: '"+pName+"' is not registered.");
    }
    if (m_mapABRTPlugins[pName]->GetType() != REPORTER)
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetReporter():"
                                            "Plugin: '"+pName+"' is not reporter plugin.");
    }
    return (CReporter*)(plugin->second);
}

CAction* CPluginManager::GetAction(const std::string& pName)
{
    map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
    if (plugin == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAction():"
                                           "Action plugin: '"+pName+"' is not registered.");
    }
    if (m_mapABRTPlugins[pName]->GetType() != ACTION)
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetAction():"
                                            "Plugin: '"+pName+"' is not action plugin.");
    }
    return (CAction*)(plugin->second);
}

CDatabase* CPluginManager::GetDatabase(const std::string& pName)
{
    map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
    if (plugin == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetDatabase():"
                                           "Database plugin: '"+pName+"' is not registered.");
    }
    if (m_mapABRTPlugins[pName]->GetType() != DATABASE)
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetDatabase():"
                                            "Plugin: '"+pName+"' is not database plugin.");
    }
    return (CDatabase*)(plugin->second);
}

plugin_type_t CPluginManager::GetPluginType(const std::string& pName)
{
    map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
    if (plugin == m_mapPlugins.end())
    {
        throw CABRTException(EXCEP_PLUGIN, "CPluginManager::GetPluginType():"
                                           "Plugin: '"+pName+"' is not registered.");
    }
    return m_mapABRTPlugins[pName]->GetType();
}

vector_map_string_t CPluginManager::GetPluginsInfo()
{
    vector_map_string_t ret;
    map_abrt_plugins_t::iterator it_abrt_plugin = m_mapABRTPlugins.begin();
    for (; it_abrt_plugin != m_mapABRTPlugins.end(); it_abrt_plugin++)
    {
        map_string_t plugin_info;

        plugin_info["Enabled"] = (m_mapPlugins.find(it_abrt_plugin->second->GetName()) != m_mapPlugins.end()) ?
                                 "yes" : "no";
        plugin_info["Type"] = plugin_type_str[it_abrt_plugin->second->GetType()];
        plugin_info["Name"] = it_abrt_plugin->second->GetName();
        plugin_info["Version"] = it_abrt_plugin->second->GetVersion();
        plugin_info["Description"] = it_abrt_plugin->second->GetDescription();
        plugin_info["Email"] = it_abrt_plugin->second->GetEmail();
        plugin_info["WWW"] = it_abrt_plugin->second->GetWWW();
        plugin_info["GTKBuilder"] = it_abrt_plugin->second->GetGTKBuilder();
        ret.push_back(plugin_info);
    }
    return ret;
}

void CPluginManager::SetPluginSettings(const std::string& pName,
                                       const std::string& pUID,
                                       const map_plugin_settings_t& pSettings)
{
    map_abrt_plugins_t::iterator abrt_plugin = m_mapABRTPlugins.find(pName);
    if (abrt_plugin != m_mapABRTPlugins.end())
    {
        map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
        if (plugin != m_mapPlugins.end())
        {
            plugin->second->SetSettings(pSettings);

            if (abrt_plugin->second->GetType() == REPORTER)
            {
                std::string home = get_home_dir(atoi(pUID.c_str()));
                if (home != "")
                {
                    std::string confDir = home + "/.abrt";
                    std::string confPath = confDir + "/" + pName + "."PLUGINS_CONF_EXTENSION;
                    uid_t uid = atoi(pUID.c_str());
                    struct passwd* pw = getpwuid(uid);
                    gid_t gid = pw ? pw->pw_gid : uid;

                    struct stat buf;
                    if (stat(confDir.c_str(), &buf) != 0)
                    {
                        if (mkdir(confDir.c_str(), 0700) == -1)
                        {
                            perror_msg("Can't create dir '%s'", confDir.c_str());
                            return;
                        }
                        if (chmod(confDir.c_str(), 0700) == -1)
                        {
                            perror_msg("Can't change mod of dir '%s'", confDir.c_str());
                            return;
                        }
                        if (chown(confDir.c_str(), uid, gid) == -1)
                        {
                            perror_msg("Can't change '%s' ownership to %u:%u", confPath.c_str(), (int)uid, (int)gid);
                            return;
                        }

                    }
                    else if (!S_ISDIR(buf.st_mode))
                    {
                        perror_msg("'%s' is not a directory", confDir.c_str());
                        return;
                    }

                    SavePluginSettings(confPath, pSettings);
                    if (chown(confPath.c_str(), uid, gid) == -1)
                    {
                        perror_msg("Can't change '%s' ownership to %u:%u", confPath.c_str(), (int)uid, (int)gid);
                        return;
                    }
                }
            }
        }
    }
}

map_plugin_settings_t CPluginManager::GetPluginSettings(const std::string& pName,
                                                        const std::string& pUID)
{
    map_plugin_settings_t ret;
    map_abrt_plugins_t::iterator abrt_plugin = m_mapABRTPlugins.find(pName);
    if (abrt_plugin != m_mapABRTPlugins.end())
    {
        map_plugins_t::iterator plugin = m_mapPlugins.find(pName);
        if (plugin != m_mapPlugins.end())
        {
            ret = plugin->second->GetSettings();

            if (abrt_plugin->second->GetType() == REPORTER)
            {
                std::string home = get_home_dir(atoi(pUID.c_str()));
                if (home != "")
                {
                    LoadPluginSettings(home + "/.abrt/" + pName + "."PLUGINS_CONF_EXTENSION, ret);
                }
            }
            return ret;
        }
    }
    return ret;
}
