/*
    CrashCatcherPlugin.cpp

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

#include "CrashCatcherPlugin.h"

CCrashCatcherPlugin::CCrashCatcherPlugin(const std::string& pLibPath) :
    m_pDynamicLibrary(NULL),
    m_pPluginInfo(NULL),
    m_pFnPluginNew(NULL)
{
    try
    {
        m_pDynamicLibrary = new CDynamicLibrary(pLibPath);
        if (m_pDynamicLibrary == NULL)
        {
            throw std::string("Not enought memory.");
        }
        m_pPluginInfo = (p_plugin_info_t) m_pDynamicLibrary->FindSymbol("plugin_info");
        m_pFnPluginNew = (p_fn_plugin_new_t) m_pDynamicLibrary->FindSymbol("plugin_new");
    }
    catch (...)
    {
        throw;
    }
}

CCrashCatcherPlugin::~CCrashCatcherPlugin()
{
    if (m_pDynamicLibrary != NULL)
    {
        delete m_pDynamicLibrary;
    }
}

const std::string& CCrashCatcherPlugin::GetVersion()
{
    return m_pPluginInfo->m_sVersion;
}

const int CCrashCatcherPlugin::GetMagicNumber()
{
    return m_pPluginInfo->m_nMagicNumber;
}

const std::string& CCrashCatcherPlugin::GetName()
{
    return m_pPluginInfo->m_sName;
}

const std::string& CCrashCatcherPlugin::GetDescription()
{
    return m_pPluginInfo->m_sDescription;
}

const std::string& CCrashCatcherPlugin::GetEmail()
{
    return m_pPluginInfo->m_sEmail;
}

const std::string& CCrashCatcherPlugin::GetWWW()
{
    return m_pPluginInfo->m_sWWW;
}

const plugin_type_t CCrashCatcherPlugin::GetType()
{
    return m_pPluginInfo->m_Type;
}

CPlugin* CCrashCatcherPlugin::PluginNew()
{
    return m_pFnPluginNew();
}
