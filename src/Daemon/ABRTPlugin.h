/*
    ABRTPlugin.h - header file for abrt plugin.

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


#ifndef ABRTPLUGIN_H_
#define ABRTPLUGIN_H_

#include <string>
#include "Plugin.h"

/**
 * CABRTPlugin class. A class which contains a loaded plugin.
 */
class CABRTPlugin
{
    private:
        /**
         * dlopen'ed library
         */
        void* m_pHandle;
        /**
         * A pointer to struc containing information about plugin.
         */
        const plugin_info_t* m_pPluginInfo;
        /**
         * A pointer to function, which creates new instances of plugin.
         */
        CPlugin* (*m_pFnPluginNew)();

    public:
        /**
         * A constructor.
         * The constructor loads a plugin
         * @param pLibPath a path to a plugin
         */
        CABRTPlugin(const char* pLibPath);
        /**
         * A destructor.
         */
        ~CABRTPlugin();
        /**
         * It is used for getting loaded plugin's version.
         * @return plugin version
         */
        const char* GetVersion();
        /**
         * It is used for getting loaded plugin's magic number.
         * @return magic number
         */
        int GetMagicNumber();
        /**
         * It is used for getting loaded plugin's name.
         * @return magic number
         */
        const char* GetName();
        /**
         * It is used for getting loaded plugin's description.
         * @return magic number
         */
        const char* GetDescription();
        /**
         * It is used for getting an author email of loaded plugin.
         * @return description
         */
        const char* GetEmail();
        /**
         * It is used for getting a home page of loaded plugin.
         * @return home page
         */
        const char* GetWWW();
        /**
         * It is used for getting a path to gui description.
         * @return home page
         */
        const char* GetGTKBuilder();
        /**
         * It is used for getting loaded plugin's type.
         * @return type
         */
        plugin_type_t GetType();
        /**
         * It is used fot getting of a new instance of loaded plugin
         * @return pointer to new allocated instance of plugin. A caller
         * has to delete it.
         */
        CPlugin* PluginNew();
};

inline
const char* CABRTPlugin::GetVersion()
{
    return m_pPluginInfo->m_sVersion;
}

inline
int CABRTPlugin::GetMagicNumber()
{
    return m_pPluginInfo->m_nMagicNumber;
}

inline
const char* CABRTPlugin::GetName()
{
    return m_pPluginInfo->m_sName;
}

inline
const char* CABRTPlugin::GetDescription()
{
    return m_pPluginInfo->m_sDescription;
}

inline
const char* CABRTPlugin::GetEmail()
{
    return m_pPluginInfo->m_sEmail;
}

inline
const char* CABRTPlugin::GetWWW()
{
    return m_pPluginInfo->m_sWWW;
}

inline
const char* CABRTPlugin::GetGTKBuilder()
{
    return m_pPluginInfo->m_sGTKBuilder;
}

inline
plugin_type_t CABRTPlugin::GetType()
{
    return m_pPluginInfo->m_Type;
}

inline
CPlugin* CABRTPlugin::PluginNew()
{
    return m_pFnPluginNew();
}

#endif /*ABRTPLUGIN_H_*/
