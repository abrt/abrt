/*
    Reporter.h - header file for reporter plugin

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

#ifndef REPORTER_H_
#define REPORTER_H_

#include <string>
#include "Plugin.h"
#include "CrashTypes.h"

/**
 * An abstract class. The class defines a reporter plugin interface.
 */
class CReporter : public CPlugin
{
    public:
        /**
         * A method, which reports a crash report to particular receiver.
         * The plugin can takes arguments, but the plugin  has to parse them
         * by itself.
         * @param pCrashReport A crash report.
         * @param pArgs Plugin's arguments.
         * @retun A message which can be displayed after a report is created.
         */
        virtual std::string Report(const map_crash_report_t& pCrashReport,
                                   const map_plugin_settings_t& pSettings,
                                   const std::string& pArgs) = 0;
};

#endif /* REPORTER_H_ */
