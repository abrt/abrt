/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

#ifndef ABRT_REPORTER_HELLO_WORLD_H
#define ABRT_REPORTER_HELLO_WORLD_H

/*
 * If you want to build reporter you have to include `reporter'.
 * Others are analyzer.h, database.h and action.h
 */
#include <abrt/reporter.h>

class CHelloWorld : public CReporter
{
    private:
        /*
         * In our tutorial we will have two options called OptionBool
         * and PrintString. Daemon will load HelloWorld.conf and pass it to our
         * SetSettings method in parsed form, allowing us to change options.
         */
        bool m_OptionBool;
        std::string m_PrintString;

    public:
        /**
        * A method, which reports a crash dump to particular receiver.
        * The plugin can take arguments, but the plugin  has to parse them
        * by itself.
        * @param pCrashData A crash report.
        * @oaran pSettings A settings passed from gui or cli
        * @param pArgs Plugin's arguments.
        * @retun A message which can be displayed after a report is created.
        */
        virtual std::string Report(const map_crash_data_t& pCrashData,
                                   const map_plugin_settings_t& pSettings,
                                   const char *pArgs);

        /**
        * A method, which takes settings and apply them. It is not a mandatory method.
        * @param pSettings Plugin's settings
        */
        virtual void SetSettings(const map_plugin_settings_t& pSettings);
};

#endif
