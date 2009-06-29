/*
    DynamicLybrary.h - header file for dynamic lybrarby wraper. It uses libdl.

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


#ifndef DYNAMICLIBRARYH_
#define DYNAMICLIBRARYH_

#include <string>

/**
 * A class. It contains one dynamic library.
 */
class CDynamicLibrary
{
    private:
        /**
         * A pointer to library.
         */
        void* m_pHandle;
        /**
         * A method, which loads a library.
         * @param pPath A path to the library.
         */
        void Load(const std::string& pPath);
    public:
        /**
         * A constructor.
         * @param pPath A path to the library.
         */
        CDynamicLibrary(const std::string& pPath);
        /**
         * A destructor.
         */
        ~CDynamicLibrary();
        /**
         * A method, which tries to find a symbol in a library. If it successes
         * then a non-NULL pointer is returned, otherwise NULL is returned.
         * @param pName A symbol name.
         * @return A pointer where a symbol name is loaded.
         */
        void* FindSymbol(const std::string& pName);
};

#endif /*DYNAMICLIBRARYH_*/
