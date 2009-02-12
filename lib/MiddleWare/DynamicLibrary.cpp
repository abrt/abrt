/*
    DynamicLybrary.cpp

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

#include "DynamicLibrary.h"
#include <iostream>
#include <dlfcn.h>

CDynamicLibrary::CDynamicLibrary(const std::string& pPath) :
    m_pHandle(NULL)
{
    Load(pPath);
}

CDynamicLibrary::~CDynamicLibrary()
{
    if (m_pHandle != NULL)
    {
        dlclose(m_pHandle);
        m_pHandle = NULL;
    }
}

void CDynamicLibrary::Load(const std::string& pPath)
{
    m_pHandle = dlopen(pPath.c_str(), RTLD_NOW);
    if (m_pHandle == NULL)
    {
        throw "CDynamicLibrary::Load(): Cannot load " + pPath + " : " + std::string(dlerror());
    }
}

void* CDynamicLibrary::FindSymbol(const std::string& pName)
{
    void* sym = dlsym(m_pHandle, pName.c_str());
    if (sym == NULL)
    {
        throw "CDynamicLibrary::Load(): Cannot find symbol '" + pName + "'";
    }
    return sym;
}
