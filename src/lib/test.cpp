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
#include "CrashTypes.h"
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>


int main(int argc, char** argv)
{

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <DebugDumpDir>" << std::endl;
        return -1;
    }
    try
    {
        CMiddleWare middleWare(PLUGINS_CONF_DIR,
                               PLUGINS_LIB_DIR);
        vector_map_string_t loaded_plugins;
        middleWare.RegisterPlugin("CCpp");
        middleWare.RegisterPlugin("Mailx");
        middleWare.RegisterPlugin("Logger");
        middleWare.RegisterPlugin("SQLite3");
        middleWare.SetDatabase("SQLite3");
        middleWare.SetOpenGPGCheck(false);
        middleWare.AddActionOrReporter("Logger", "");
        middleWare.AddAnalyzerActionOrReporter("CCpp", "Mailx", "");

        loaded_plugins = middleWare.GetPluginsInfo();
        std::cout << "Loaded plugins" << std::endl;
        int ii;
        for ( ii = 0; ii < loaded_plugins.size(); ii++)
        {
            std::cout << "-------------------------------------------" << std::endl;
            map_plugin_settings_t settings;
            std::cout << "Enabled: " << loaded_plugins[ii]["Enabled"] << std::endl;
            std::cout << "Type: " << loaded_plugins[ii]["Type"] << std::endl;
            std::cout << "Name: " << loaded_plugins[ii]["Name"] << std::endl;
            std::cout << "Version: " << loaded_plugins[ii]["Version"] << std::endl;
            std::cout << "Description: " << loaded_plugins[ii]["Description"] << std::endl;
            std::cout << "Email: " << loaded_plugins[ii]["Email"] << std::endl;
            std::cout << "WWW: " << loaded_plugins[ii]["WWW"] << std::endl;
            std::cout << "GTKBuilder: " << loaded_plugins[ii]["GTKBuilder"] << std::endl;
            if (loaded_plugins[ii]["Enabled"] == "yes")
            {
                std::cout << std::endl << "Settings: " << std::endl;
                settings = middleWare.GetPluginSettings(loaded_plugins[ii]["Name"]);
                map_plugin_settings_t::iterator it;
                for (it = settings.begin(); it != settings.end(); it++)
                {
                    std::cout << "\t" << it->first << ": " << it->second << std::endl;
                }
            }
            std::cout << "-------------------------------------------" << std::endl;
        }
        /* Try to save it into DB */
        map_crash_data_t crashInfo;
        if (middleWare.SaveDebugDump(argv[1], crashInfo))
        {
            std::cout << "Application Crashed! " <<
                         crashInfo[FILENAME_PACKAGE][CD_CONTENT] << ", " <<
                         crashInfo[FILENAME_EXECUTABLE][CD_CONTENT] << ", " <<
                         crashInfo[FILENAME_COUNT][CD_CONTENT] << ", " << std::endl;

            /* Get Report, so user can change data (remove private stuff)
             * If we do not want user interaction, just send data immediately
             */
            map_crash_data_t crashReport;
            middleWare.CreateCrashReport(crashInfo[FILENAME_DUPHASH][CD_CONTENT],
                                         crashInfo[FILENAME_UID][CD_CONTENT],
                                         crashReport);
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
