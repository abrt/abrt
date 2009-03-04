/*
    ABRTPlugin.h - header file for abrt plugin. It takes care
                           of reporting thinks which has loaded plugin.

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
#include "DynamicLibrary.h"
#include "Plugin.h"

class CABRTPlugin
{
    private:

        typedef const plugin_info_t* p_plugin_info_t;
        typedef CPlugin* (*p_fn_plugin_new_t)();

        CDynamicLibrary* m_pDynamicLibrary;
        p_plugin_info_t m_pPluginInfo;
        p_fn_plugin_new_t m_pFnPluginNew;

    public:
        CABRTPlugin(const std::string& pLibPath);
        ~CABRTPlugin();

        const std::string& GetVersion();
        const int GetMagicNumber();
        const std::string& GetName();
        const std::string& GetDescription();
        const std::string& GetEmail();
        const std::string& GetWWW();
        const plugin_type_t GetType();

        CPlugin* PluginNew();
};

#endif /*ABRTPLUGIN_H_*/
