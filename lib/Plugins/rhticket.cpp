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

#define _GNU_SOURCE 1    /* for stpcpy */
#include "abrtlib.h"
#include "abrt_curl.h"
#include "abrt_rh_support.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "rhticket.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

using namespace std;


#if 0 //unused
static char *xml_escape(const char *str)
{
    const char *s = str;
    unsigned count = 1; /* for NUL */
    while (*s)
    {
        if (*s == '&')
            count += sizeof("&amp;")-2;
        if (*s == '<')
            count += sizeof("&lt;")-2;
        if (*s == '>')
            count += sizeof("&gt;")-2;
        if ((unsigned char)*s > 126 || (unsigned char)*s < ' ')
            count += sizeof("\\x00")-2;
        count++;
        s++;
    }
    char *result = (char*)xmalloc(count);
    char *d = result;
    s = str;
    while (*s)
    {
        if (*s == '&')
            d = stpcpy(d, "&amp;");
        else if (*s == '<')
            d = stpcpy(d, "&lt;");
        else if (*s == '>')
            d = stpcpy(d, "&gt;");
        else
        if ((unsigned char)*s > 126
         || (  (unsigned char)*s < ' '
            && *s != '\t'
            && *s != '\n'
            && *s != '\r'
            )
        ) {
            *d++ = '\\';
            *d++ = 'x';
            *d++ = "0123456789abcdef"[(unsigned char)*s >> 4];
            *d++ = "0123456789abcdef"[(unsigned char)*s & 0xf];
        }
        else
            *d++ = *s;
        s++;
    }
    *d = '\0';
    return result;
}
#endif


/*
 * CReporterRHticket
 */

CReporterRHticket::CReporterRHticket() :
    m_bSSLVerify(true),
    m_sStrataURL("http://support-services-devel.gss.redhat.com:8080/Strata")
{}

CReporterRHticket::~CReporterRHticket()
{}

string CReporterRHticket::Report(const map_crash_data_t& pCrashData,
        const map_plugin_settings_t& pSettings,
        const char *pArgs)
{
    const string& package   = get_crash_data_item_content(pCrashData, FILENAME_PACKAGE);
//  const string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
//  const string& release   = get_crash_data_item_content(pCrashData, FILENAME_RELEASE);
//  const string& arch      = get_crash_data_item_content(pCrashData, FILENAME_ARCHITECTURE);
//  const string& duphash   = get_crash_data_item_content(pCrashData, CD_DUPHASH);
    const char *reason      = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_REASON);
    const char *function    = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_CRASH_FUNCTION);

    string summary = "[abrt] " + package;
    if (function && strlen(function) < 30)
    {
        summary += ": ";
        summary += function;
    }
    if (reason)
    {
        summary += ": ";
        summary += reason;
    }

    string description = "abrt version: "VERSION"\n";
    description += make_description_bz(pCrashData);

    reportfile_t* file = new_reportfile();

    // TODO: some files are totally useless:
    // "Reported", "Message" (plugin's output), "DumpDir",
    // "Description" (package description) - maybe skip those?
    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
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

    update_client(_("Creating a new case..."));
//    const char* filename = reportfile_as_file(file);
    char* result = send_report_to_new_case(m_sStrataURL.c_str(),
            m_sLogin.c_str(),
            m_sPassword.c_str(),
            summary.c_str(),
            description.c_str(),
            package.c_str(),
            "/dev/null" //filename
    );
    VERB3 log("post result:'%s'", result);
    string retval = result;
    reportfile_free(file);
    free(result);

    if (strncasecmp(retval.c_str(), "error", 5) == 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "%s", retval.c_str());
    }
    return retval;
}

void CReporterRHticket::SetSettings(const map_plugin_settings_t& pSettings)
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
const map_plugin_settings_t& CReporterRHticket::GetSettings()
{
    m_pSettings["URL"] = m_sStrataURL;
    m_pSettings["Login"] = m_sLogin;
    m_pSettings["Password"] = m_sPassword;
    m_pSettings["SSLVerify"] = m_bSSLVerify ? "yes" : "no";

    return m_pSettings;
}

PLUGIN_INFO(REPORTER,
    CReporterRHticket,
    "RHticket",
    "0.0.4",
    "Reports bugs to Red Hat support",
    "Denys Vlasenko <dvlasenk@redhat.com>",
    "https://fedorahosted.org/abrt/wiki",
    "" /*PLUGINS_LIB_DIR"/rhticket.GTKBuilder"*/);
