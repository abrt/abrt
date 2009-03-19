/*
    Analyzer.h - header file for analyzer plugin

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

#ifndef ANALYZER_H_
#define ANALYZER_H_

#include <string>
#include "Plugin.h"

class CAnalyzer : public CPlugin
{
    public:
        virtual ~CAnalyzer() {}
        virtual std::string GetLocalUUID(const std::string& pDebugDumpPath) = 0;
        virtual std::string GetGlobalUUID(const std::string& pDebugDumpPath) = 0;
        virtual void CreateReport(const std::string& pDebugDumpPath) = 0;
};

#endif /*ANALYZER_H_*/
