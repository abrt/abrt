/*
    MiddleWare.h - header file for MiddleWare library. It wraps plugins and
                   take case of them.

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


#ifndef MIDDLEWARE_H_
#define MIDDLEWARE_H_

#include "PluginManager.h"

class CMiddleWare
{
	private:

		CPluginManager* m_PluginManager;

	public:
		CMiddleWare(const std::string& pPlugisConfDir,
				    const std::string& pPlugisLibDir);

		~CMiddleWare();

		void LoadPlugins();
		void LoadPlugin(const std::string& pName);
		void UnLoadPlugin(const std::string& pName);

		std::string GetUUID(const std::string& pLanguage, void* pData);
		std::string GetReport(const std::string& pLanguage, void* pData);
		int Report(const std::string& pReporter, const std::string& pDebugDumpPath);
		//void SaveDebugDumpToDataBase(const std::string& pPath);
};

#endif /*MIDDLEWARE_H_*/
