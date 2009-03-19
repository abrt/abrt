/*
    Logger.h - header file for Logger reporter plugin
             - it simple writes report to specific file

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

#ifndef LOGGER_H_
#define LOGGER_H_

#include "Plugin.h"
#include "Reporter.h"

class CLogger : public CReporter
{
    private:
        std::string m_sLogPath;
        bool m_bAppendLogs;
    public:
        CLogger();
        virtual ~CLogger() {}

        void LoadSettings(const std::string& pPath);
        void Report(const crash_report_t& pReport);
};


PLUGIN_INFO(REPORTER,
            CLogger,
            "Logger",
            "0.0.1",
            "Write a report to a specific file",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/crash-catcher/wiki");

#endif /* LOGGER_H_ */
