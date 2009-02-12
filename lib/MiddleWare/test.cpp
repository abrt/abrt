/*
    test.cpp - simple library test

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
#include "DebugDump.h"
#include <iostream>
#include <sys/types.h>
#include <unistd.h>


int main(int argc, char** argv)
{


    try
    {
        CMiddleWare middleWare(PLUGINS_CONF_DIR,
                               PLUGINS_LIB_DIR,
                               std::string(CONF_DIR) + "/CrashCatcher.conf");
        /* Create DebugDump */
        CDebugDump dd;
        char pid[100];
        sprintf(pid, "%d", getpid());
        dd.Create(std::string(DEBUG_DUMPS_DIR)+"/"+pid, pid);
        dd.SaveText(FILENAME_LANGUAGE, "CCpp");
        dd.SaveBinary(FILENAME_BINARYDATA1, "ass0-9as", sizeof("ass0-9as"));

        /* Try to save it into DB */
        CMiddleWare::crash_info_t info;
        if (middleWare.SaveDebugDump(std::string(DEBUG_DUMPS_DIR)+"/"+pid, info))
        {
            std::cout << "Application Crashed! " <<
                         "(" << info.m_sTime << " [" << info.m_sCount << "]) " <<
                         info.m_sPackage << ": " <<
                         info.m_sExecutable << std::endl;

            /* Get Report, so user can change data (remove private stuff)
             * If we do not want user interaction, just send data immediately
             */
            CMiddleWare::crash_report_t crashReport;
            middleWare.CreateReport(info.m_sUUID, info.m_sUID, crashReport);
            /* Report crash */
            middleWare.Report(crashReport);
        }
    }
    catch (std::string sError)
    {
        std::cerr << sError << std::endl;
    }

    return 0;
}
