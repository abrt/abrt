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

#include <abrt/abrtlib.h>
#include "abrt-reporter-hello-world.h"

std::string CHelloWorld::Report(const map_crash_data_t& pCrashData,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    if (!m_OptionBool)
    {
        /*
         * Exceptions is used to notify gui/cli that something is wrong
         * and stop reporting; gui/cli will show it as an error message.
         */
        throw CABRTException(EXCEP_PLUGIN, "OptionBool is set to `no', wrong");
    }

    /*
     * Same as example above, but here we show how you can use try..catch block
     */
    try
    {
    }
    catch (CABRTException& e)
    {
        //throw CABRTException(EXCEP_PLUGIN, "Oops something wrong");
    }

    /*
     * If you want to log some information use function log.
     * abrtd can be started as standalone application running in
     * foreground (abrtd -d) and then you will see log information.
     * Logging has tree levels and they are controlled by VERBx where `x'
     * can be 1, 2 and 3. If you want to level1 (VERB1 message) run abrtd -dv,
     * for level2 (VERB2 message) -dvv and for level3(VERB3 message) -dvvv.
     */

     VERB1 log("what you sometimes want to see, even on a production box");
     VERB2 log("debug message, not going into insanely small details");
     VERB3 log("lots and lots of details");

    return m_PrintString;
}

void CHelloWorld::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;

    it = pSettings.find("OptionBool");
    if (it != end)
        m_OptionBool = string_to_bool(it->second.c_str());

    it = pSettings.find("PrintString");
    if (it != end)
        m_PrintString = it->second;

}

/*
 * Use the macro PLUGIN_INFO in the *.cpp file of your plugin so that your
 * subclass will be properly registered and treated as a plugin.
 * This sets up all the lower-level and administrative details to fit your
 * class into the plugin infrastructure. The syntax is:

 * PLUGIN_INFO(type, plugin_class, name, version, description, email, www, gtk_builder)
 *  - "type" is one of ANALYZER, ACTION, REPORTER, or DATABASE
 *  - "plugin_class" is the identifier of the class
 *  - "name" is a string with the name of the plugin
 *  - "version" is a string with the version of the plugin
 *  - "description" is a string with the summary of what the plugin does
 *  - "email" and "www" are strings with the contact info for the author
 *  - "gtk_builder" is path to plugins gui
 */

PLUGIN_INFO(REPORTER,
            CHelloWorld,
            "HelloWorld",
            "0.0.1",
            "Show `Hello world!!!' in gui",
            "author",
            "www adress to project/plugin",
            "/path/to/gui.plugin")
