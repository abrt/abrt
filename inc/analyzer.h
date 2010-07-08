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
#include "plugin.h"

/**
 * An abstract class. The class defines an analyzer plugin interface.
 */
class CAnalyzer : public CPlugin
{
    public:
        /**
         * A method, which gets a local UUID of particular crash. The local
         * UUID is usualy computed from data which are stored in debugdump dir.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         * @return A local UUID.
         */
        virtual std::string GetLocalUUID(const char *pDebugDumpDir) = 0;
        /**
         * A method, which gets a global UUID of particular crash.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         * @return A global UUID.
         */
        virtual std::string GetGlobalUUID(const char *pDebugDumpDir) = 0;
        /**
         * A method, which takes care of getting all additional data needed
         * for computing UUIDs and creating a report. This report could be send
         * somewhere afterwards.
         * @param pDebugDumpPath A debugdump dir containing all necessary data.
         */
        virtual void CreateReport(const char *pDebugDumpDir, int force) = 0;
};

#endif /*ANALYZER_H_*/
