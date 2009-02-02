/*
    MiddleWare.cpp

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

#include "MiddleWare.h"

CMiddleWare::CMiddleWare(const std::string& pPlugisConfDir,
					     const std::string& pPlugisLibDir) :
	m_PluginManager(NULL)
{
	m_PluginManager = new CPluginManager(pPlugisConfDir, pPlugisLibDir);
	if (m_PluginManager == NULL)
	{
		throw std::string("Not enought memory.");
	}
}

CMiddleWare::~CMiddleWare()
{
	if (m_PluginManager != NULL)
	{
		delete m_PluginManager;
	}
}

void CMiddleWare::LoadPlugins()
{
	m_PluginManager->LoadPlugins();
}

void CMiddleWare::LoadPlugin(const std::string& pName)
{
	m_PluginManager->LoadPlugin(pName);
}

void CMiddleWare::UnLoadPlugin(const std::string& pName)
{
	m_PluginManager->UnLoadPlugin(pName);
}

std::string CMiddleWare::GetUUID(const std::string& pLanguage, void* pData)
{
	CLanguage* language = m_PluginManager->GetLanguage(pLanguage);
	if (language == NULL)
	{
		return "";
	}
	return language->GetUUID(pData);
}

std::string CMiddleWare::GetReport(const std::string& pLanguage, void* pData)
{
	CLanguage* language = m_PluginManager->GetLanguage(pLanguage);
	if (language == NULL)
	{
		return "";
	}
	return language->GetReport(pData);
}

int CMiddleWare::Report(const std::string& pReporter, const std::string& pDebugDumpPath)
{
	CReporter* reporter = m_PluginManager->GetReporter(pReporter);
	if (reporter == NULL)
	{
		return -1;
	}
	reporter->Report(pDebugDumpPath);
}

