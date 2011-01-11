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
#include <dlfcn.h>
#include "abrtlib.h"
#include "PluginManager.h"

using namespace std;


/**
 * CLoadedModule class. A class which contains a loaded plugin.
 */
class CLoadedModule
{
    private:
        /* dlopen'ed library */
        void *m_pHandle;
        const plugin_info_t *m_pPluginInfo;
        CPlugin* (*m_pFnPluginNew)();

    public:
        CLoadedModule(void *handle, const char *mod_name);
        ~CLoadedModule()             { dlclose(m_pHandle); }
        int GetMagicNumber()         { return m_pPluginInfo->m_nMagicNumber; }
        const char *GetVersion()     { return m_pPluginInfo->m_sVersion; }
        const char *GetName()        { return m_pPluginInfo->m_sName; }
        const char *GetDescription() { return m_pPluginInfo->m_sDescription; }
        const char *GetEmail()       { return m_pPluginInfo->m_sEmail; }
        const char *GetWWW()         { return m_pPluginInfo->m_sWWW; }
        const char *GetGTKBuilder()  { return m_pPluginInfo->m_sGTKBuilder; }
        plugin_type_t GetType()      { return m_pPluginInfo->m_Type; }
        CPlugin *PluginNew()         { return m_pFnPluginNew(); }
};
CLoadedModule::CLoadedModule(void *handle, const char *mod_name)
{
    m_pHandle = handle;
    /* All errors are fatal */
#define LOADSYM(fp, handle, name) \
    do { \
        fp = (typeof(fp)) (dlsym(handle, name)); \
        if (!fp) \
            error_msg_and_die("'%s' has no %s entry", mod_name, name); \
    } while (0)

    LOADSYM(m_pPluginInfo, handle, "plugin_info");
    LOADSYM(m_pFnPluginNew, handle, "plugin_new");
#undef LOADSYM
}


/**
 * Text representation of plugin types.
 */
static const char *const plugin_type_str[] = {
    "Analyzer",
    "Action",
    "Reporter",
    "Database"
};


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
            if (!is_regular_file(dent, PLUGINS_LIB_DIR))
                continue;
            char *ext = strrchr(dent->d_name, '.');
            if (!ext || strcmp(ext + 1, PLUGINS_LIB_EXTENSION) != 0)
                continue;
            *ext = '\0';
            if (strncmp(dent->d_name, PLUGINS_LIB_PREFIX, sizeof(PLUGINS_LIB_PREFIX)-1) != 0)
                continue;
            LoadPlugin(dent->d_name + sizeof(PLUGINS_LIB_PREFIX)-1, /*enabled_only:*/ true);
        }
        closedir(dir);
    }
}

void CPluginManager::UnLoadPlugins()
{
    map_loaded_module_t::iterator it_module;
    while ((it_module = m_mapLoadedModules.begin()) != m_mapLoadedModules.end())
    {
        UnLoadPlugin(it_module->first.c_str());
    }
}

CPlugin* CPluginManager::LoadPlugin(const char *pName, bool enabled_only)
{
    map_plugin_t::iterator it_plugin = m_mapPlugins.find(pName);
    if (it_plugin != m_mapPlugins.end())
    {
        return it_plugin->second; /* ok */
    }

    map_string_t plugin_info;
    plugin_info["Name"] = pName;

    const char *conf_name = pName;
    if (strncmp(pName, "Kerneloops", sizeof("Kerneloops")-1) == 0)
    {
        /* Kerneloops{,Scanner,Reporter} share the same .conf file */
        conf_name = "Kerneloops";
    }
    char *conf_fullname = xasprintf(PLUGINS_CONF_DIR"/%s."PLUGINS_CONF_EXTENSION, conf_name);
    map_string_h *pluginSettings = new_map_string();
    if (load_conf_file(conf_fullname, pluginSettings, /*skip key w/o values:*/ true))
        VERB3 log("Loaded %s.conf", conf_name);
    free(conf_fullname);

    if (enabled_only)
    {
        if (!string_to_bool(get_map_string_item_or_empty(pluginSettings, "Enabled")))
        {
            plugin_info["Enabled"] = "no";
            string empty;
            plugin_info["Type"] = empty;
            plugin_info["Version"] = empty;
            plugin_info["Description"] = empty;
            plugin_info["Email"] = empty;
            plugin_info["WWW"] = empty;
            plugin_info["GTKBuilder"] = empty;
            VERB3 log("Plugin %s: 'Enabled' is not set, not loading it (yet)", pName);
            free_map_string(pluginSettings);
            return NULL; /* error */
        }
    }

    char *libPath = xasprintf(PLUGINS_LIB_DIR"/"PLUGINS_LIB_PREFIX"%s."PLUGINS_LIB_EXTENSION, pName);
    void *handle = dlopen(libPath, RTLD_NOW);
    if (!handle)
    {
        error_msg("Can't load '%s': %s", libPath, dlerror());
        free(libPath);
        free_map_string(pluginSettings);
        return NULL; /* error */
    }
    free(libPath);
    CLoadedModule *module = new CLoadedModule(handle, pName);
    if (module->GetMagicNumber() != PLUGINS_MAGIC_NUMBER
     || module->GetType() < 0
     || module->GetType() > MAX_PLUGIN_TYPE
    ) {
        error_msg("Can't load non-compatible plugin %s: magic %d != %d or type %d is not in [0,%d]",
                pName,
                module->GetMagicNumber(), PLUGINS_MAGIC_NUMBER,
                module->GetType(), MAX_PLUGIN_TYPE);
        delete module;
        free_map_string(pluginSettings);
        return NULL; /* error */
    }
    VERB3 log("Loaded plugin %s v.%s", pName, module->GetVersion());

    CPlugin *plugin = NULL;
    plugin = module->PluginNew();
    plugin->Init();
    /* Need to convert pluginSettings from map_string_h container
     * to map_string_t, since plugin->SetSettings() needs that type.
     * To be removed when remaining uses of map_string_t
     * are globally converted to map_string_h.
     */
    {
        map_string_t pluginSettings2;
        GHashTableIter iter;
        char *name;
        char *value;
        g_hash_table_iter_init(&iter, pluginSettings);
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
        {
            pluginSettings2[name] = value;
        }
        plugin->SetSettings(pluginSettings2);
    }
    free_map_string(pluginSettings);

    plugin_info["Enabled"] = "yes";
    plugin_info["Type"] = plugin_type_str[module->GetType()];
    //plugin_info["Name"] = module->GetName();
    plugin_info["Version"] = module->GetVersion();
    plugin_info["Description"] = module->GetDescription();
    plugin_info["Email"] = module->GetEmail();
    plugin_info["WWW"] = module->GetWWW();
    plugin_info["GTKBuilder"] = module->GetGTKBuilder();

    m_mapLoadedModules[pName] = module;
    m_mapPlugins[pName] = plugin;
    log("Registered %s plugin '%s'", plugin_type_str[module->GetType()], pName);
    return plugin; /* ok */
}

void CPluginManager::UnLoadPlugin(const char *pName)
{
    map_loaded_module_t::iterator it_module = m_mapLoadedModules.find(pName);
    if (it_module != m_mapLoadedModules.end())
    {
        map_plugin_t::iterator it_plugin = m_mapPlugins.find(pName);
        if (it_plugin != m_mapPlugins.end()) /* always true */
        {
            it_plugin->second->DeInit();
            delete it_plugin->second;
            m_mapPlugins.erase(it_plugin);
        }
        log("UnRegistered %s plugin %s", plugin_type_str[it_module->second->GetType()], pName);
        delete it_module->second;
        m_mapLoadedModules.erase(it_module);
    }
}

CAction* CPluginManager::GetAction(const char *pName, bool silent)
{
    CPlugin *plugin = LoadPlugin(pName);
    if (!plugin)
    {
        error_msg("Plugin '%s' is not registered", pName);
        return NULL;
    }
    if (m_mapLoadedModules[pName]->GetType() != ACTION)
    {
        if (!silent)
            error_msg("Plugin '%s' is not an action plugin", pName);
        return NULL;
    }
    return (CAction*)plugin;
}

plugin_type_t CPluginManager::GetPluginType(const char *pName)
{
    CPlugin *plugin = LoadPlugin(pName);
    if (!plugin)
    {
        return INVALID_PLUGIN_TYPE;
    }
    map_loaded_module_t::iterator it_module = m_mapLoadedModules.find(pName);
    return it_module->second->GetType();
}
