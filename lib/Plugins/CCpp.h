/*
    CCpp.h - header file for C/C++ language plugin
           - it can ger UUID and memory maps from core files

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

#ifndef CCPP_H_
#define CCPP_H_

#include <string>
#include "Plugin.h"
#include "Language.h"

class CLanguageCCpp : public CLanguage
{
	private:
		bool m_bMemoryMap;
		std::string m_sOldCorePattern;
	public:
		CLanguageCCpp();
		virtual ~CLanguageCCpp() {}
		std::string GetLocalUUID(const std::string& pDebugDumpPath);
		std::string GetReport(const std::string& pDebugDumpPath);
		void Init();
		void DeInit();
		void SetSettings(const map_settings_t& pSettings);
};


PLUGIN_INFO(LANGUAGE,
			"CCpp",
			"0.0.1",
		    "Simple C/C++ language plugin.",
		    "zprikryl@redhat.com",
		    "https://fedorahosted.org/crash-catcher/wiki");

PLUGIN_INIT(CLanguageCCpp);

#endif /* CCPP */
