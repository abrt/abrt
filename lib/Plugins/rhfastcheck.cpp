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

#include "abrtlib.h"
#include "abrt_rh_support.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "rhfastcheck.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

using namespace std;

/*
 * CReporterRHfastcheck
 */
CReporterRHfastcheck::CReporterRHfastcheck() :
    m_bSSLVerify(true),
    m_sStrataURL("http://support-services-devel.gss.redhat.com:8080/Strata")
{}

CReporterRHfastcheck::~CReporterRHfastcheck()
{}

string CReporterRHfastcheck::Report(const map_crash_data_t& pCrashData,
        const map_plugin_settings_t& pSettings,
        const char *pArgs)
{
    reportfile_t* file = new_reportfile();

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        if (it->first == CD_COUNT) continue;
        if (it->first == CD_DUMPDIR) continue;
        if (it->first == CD_INFORMALL) continue;
        if (it->first == CD_REPORTED) continue;
        if (it->first == CD_MESSAGE) continue; // plugin's status message (if we already reported it yesterday)
        if (it->first == FILENAME_DESCRIPTION) continue; // package description

        const char *content = it->second[CD_CONTENT].c_str();
        if (it->second[CD_TYPE] == CD_TXT)
        {
            reportfile_add_binding_from_string(file, it->first.c_str(), content);
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            reportfile_add_binding_from_namedfile(file, content, it->first.c_str(), content, /*binary:*/ 1);
        }
    }

    update_client(_("Creating a signature..."));
    const char* signature = reportfile_as_string(file);
    char* result = post_signature(m_sStrataURL.c_str(), signature);

    reportfile_free(file);
    string retval = result;
    free(result);

    if (strncasecmp(retval.c_str(), "error", 5) == 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "%s", retval.c_str());
    }
    return retval;
}

void CReporterRHfastcheck::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("URL");
    if (it != end)
    {
        m_sStrataURL = it->second;
    }
    it = pSettings.find("Login");
    if (it != end)
    {
        m_sLogin = it->second;
    }
    it = pSettings.find("Password");
    if (it != end)
    {
        m_sPassword = it->second;
    }
    it = pSettings.find("SSLVerify");
    if (it != end)
    {
        m_bSSLVerify = string_to_bool(it->second.c_str());
    }
}

/* Should not be deleted (why?) */
const map_plugin_settings_t& CReporterRHfastcheck::GetSettings()
{
    m_pSettings["URL"] = m_sStrataURL;
    m_pSettings["Login"] = m_sLogin;
    m_pSettings["Password"] = m_sPassword;
    m_pSettings["SSLVerify"] = m_bSSLVerify ? "yes" : "no";

    return m_pSettings;
}

PLUGIN_INFO(REPORTER,
    CReporterRHfastcheck,
    "RHfastcheck",
    "0.0.4",
    "Reports bugs to Red Hat support",
    "Denys Vlasenko <dvlasenk@redhat.com>",
    "https://fedorahosted.org/abrt/wiki",
    "" /*PLUGINS_LIB_DIR"/RHfastcheck.GTKBuilder"*/);
