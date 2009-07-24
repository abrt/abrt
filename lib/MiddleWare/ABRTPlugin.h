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
#include "DynamicLibrary.h"
#include "Plugin.h"

/**
 * CABRTPlugin class. A class which contains a loaded plugin.
 */
class CABRTPlugin
{
    private:

        typedef const plugin_info_t* p_plugin_info_t;
        typedef CPlugin* (*p_fn_plugin_new_t)();

        /**
         * A class containing library which contains plugin functionality.
         * @see DynamicLibrary.h
         */
        CDynamicLibrary* m_pDynamicLibrary;
        /**
         * A pointer to struc containing information about plugin.
         */
        p_plugin_info_t m_pPluginInfo;
        /**
         * A pointer to function, which creates new instances of plugin.
         */
        p_fn_plugin_new_t m_pFnPluginNew;

    public:
        /**
         * A constructor.
         * The constructor loads a plugin
         * @param pLibPath a path to a plugin
         */
        CABRTPlugin(const std::string& pLibPath);
        /**
         * A destructor.
         */
        ~CABRTPlugin();
        /**
         * It is used for getting loaded plugin's version.
         * @return plugin version
         */
        const std::string& GetVersion();
        /**
         * It is used for getting loaded plugin's magic number.
         * @return magic number
         */
        const int GetMagicNumber();
        /**
         * It is used for getting loaded plugin's name.
         * @return magic number
         */
        const std::string& GetName();
        /**
         * It is used for getting loaded plugin's description.
         * @return magic number
         */
        const std::string& GetDescription();
        /**
         * It is used for getting an author email of loaded plugin.
         * @return description
         */
        const std::string& GetEmail();
        /**
         * It is used for getting a home page of loaded plugin.
         * @return home page
         */
        const std::string& GetWWW();
        /**
         * It is used for getting a path to gui description.
         * @return home page
         */
        const std::string& GetGTKBuilder();
        /**
         * It is used for getting loaded plugin's type.
         * @return type
         */
        const plugin_type_t GetType();
        /**
         * It is used fot getting of a new instance of loaded plugin
         * @return pointer to new allocated instance of plugin. A caller
         * has to delete it.
         */
        CPlugin* PluginNew();
};

#endif /*ABRTPLUGIN_H_*/
