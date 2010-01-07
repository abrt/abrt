/*
    ABRTPlugin.cpp

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

#include "abrtlib.h"
#include "ABRTPlugin.h"
#include <dlfcn.h>

CLoadedModule::CLoadedModule(const char* pLibPath)
{
    /* All errors are fatal */
    m_pHandle = dlopen(pLibPath, RTLD_NOW);
    if (!m_pHandle)
        error_msg_and_die("can't load '%s': %s", pLibPath, dlerror());

#define LOADSYM(fp, handle, name) do { \
    fp = (typeof(fp)) (dlsym(handle, name)); \
    if (!fp) \
        error_msg_and_die("'%s' has no %s entry", pLibPath, name); \
} while (0)

    LOADSYM(m_pPluginInfo, m_pHandle, "plugin_info");
    LOADSYM(m_pFnPluginNew, m_pHandle, "plugin_new");
}

CLoadedModule::~CLoadedModule()
{
    dlclose(m_pHandle);
}
