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
#include "rhticket.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

using namespace std;


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

    string summary = "[abrt] crash in " + package;
    if (reason != NULL)
    {
        summary += ": ";
        summary += reason;
    }
//  string status_whiteboard = "abrt_hash:" + duphash;

    string description = "abrt "VERSION" detected a crash.\n\n";
    description += make_description_bz(pCrashData);

//  string product;
//  string version;
//  parse_release(release.c_str(), product, version);

    char *xml_summary = xml_escape(summary.c_str());
    char *xml_description = xml_escape(description.c_str());
    string postdata = ssprintf(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<IssueTrackerCase xmlns=\"http://www.redhat.com/gss/strata\">"
        "<name>%s</name>"
        "<description>%s</description>"
        // "<reference></reference><notes></notes><tags></tags>"
        "</IssueTrackerCase>",
        xml_summary,
        xml_description
    );
    free(xml_summary);
    free(xml_description);
    string url = concat_path_file(m_sStrataURL.c_str(), "cases");

    curl_post_state *state = new_curl_post_state(0
                + ABRT_CURL_POST_WANT_HEADERS
                + ABRT_CURL_POST_WANT_ERROR_MSG);
    int http_resp_code = curl_post(state, url.c_str(), postdata.c_str());

    if (http_resp_code / 100 != 2)
    {
        /* not 2xx */
        string errmsg = state->curl_error_msg ? state->curl_error_msg : "(none)";
        free_curl_post_state(state);
        throw CABRTException(EXCEP_PLUGIN, _("server returned HTTP code %u, error message: %s"),
                http_resp_code, errmsg.c_str());
    }

    string result = find_header_in_curl_post_state(state, "Location:") ? : "";
    free_curl_post_state(state);
    return result;
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
