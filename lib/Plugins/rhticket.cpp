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
#include "abrt_xmlrpc.h" /* for xcurl_easy_handle */
#include "rhticket.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

using namespace std;


static char*
check_curl_error(CURLcode err, const char* msg)
{
	if (err) {
		return xasprintf("%s: %s", msg, curl_easy_strerror(err));
	}
	return NULL;
}

//
// Examine each header looking for "Location:" header
//
struct Headerdata {
	char *location;
};

static size_t
headerfunction(void *buffer_pv, size_t count, size_t nmemb, void *headerdata_pv)
{
	struct Headerdata* headerdata = (struct Headerdata*)headerdata_pv;
	const char* buffer = (const char*)buffer_pv;
	const char location_key[] = "Location:";
	const size_t location_key_size = sizeof(location_key)-1;

	size_t size = count * nmemb;
	if (size >= location_key_size
	 && 0 == memcmp(buffer, location_key, location_key_size)
	) {
		const char* start = (const char*) buffer + location_key_size + 1;
		const char* end;

		// skip over any leading space
		while (start < buffer+size && isspace(*start))
			++start;

		end = start;

		// skip till we find the end of the url (first space or end of buffer)
		while (end < buffer+size && !isspace(*end))
			++end;

		headerdata->location = xstrndup(start, end - start);
	}

	return size;
}

static char*
post(int *http_resp_code, const char* url, const char* data)
{
	char *retval;
	CURLcode curl_err;
	struct curl_slist *httpheader_list = NULL;
	struct Headerdata headerdata = { NULL };

	if (http_resp_code)
		*http_resp_code = -1;

	CURL *handle = xcurl_easy_init();

	curl_err = curl_easy_setopt(handle, CURLOPT_VERBOSE, 0);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_VERBOSE)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_NOPROGRESS)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_POST, 1);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_POST)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_URL, url);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_URL)");
	if (retval)
		goto ret;

	httpheader_list = curl_slist_append(httpheader_list, "Content-Type: application/xml");
	curl_err = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, httpheader_list);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_HTTPHEADER)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_POSTFIELDS)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, headerfunction);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_HEADERFUNCTION)");
	if (retval)
		goto ret;

	curl_err = curl_easy_setopt(handle, CURLOPT_WRITEHEADER, &headerdata);
	retval = check_curl_error(curl_err, "curl_easy_setopt(CURLOPT_WRITEHEADER)");
	if (retval)
		goto ret;

	curl_err = curl_easy_perform(handle);
	retval = check_curl_error(curl_err, "curl_easy_perform");
	if (retval)
		goto ret;

	long response_code;
	curl_err = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
	retval = check_curl_error(curl_err, "curl_easy_getinfo(CURLINFO_RESPONSE_CODE)");
	if (retval)
		goto ret;

	if (http_resp_code)
		*http_resp_code = response_code;
	switch (response_code) {
	case 200:
	case 201:
		retval = headerdata.location;
		break;
	/* default: */
		/* TODO: extract meaningful error string from server reply */
	}

 ret:
	curl_easy_cleanup(handle);
	curl_slist_free_all(httpheader_list);
	return retval;
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
//	const string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
	const string& release   = get_crash_data_item_content(pCrashData, FILENAME_RELEASE);
//	const string& arch      = get_crash_data_item_content(pCrashData, FILENAME_ARCHITECTURE);
	const string& duphash   = get_crash_data_item_content(pCrashData, CD_DUPHASH);
	const char *reason      = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_REASON);

	string summary = "[abrt] crash in " + package;
	if (reason != NULL) {
		summary += ": ";
		summary += reason;
	}
//	string status_whiteboard = "abrt_hash:" + duphash;

	string description = "abrt "VERSION" detected a crash.\n\n";
	description += make_description_bz(pCrashData);

//	string product;
//	string version;
//	parse_release(release.c_str(), product, version);

	string postdata = ssprintf(
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<IssueTrackerCase xmlns=\"http://www.redhat.com/gss/strata\">"
		"<name>%s</name>"
		"<description>%s</description>"
		// "<reference></reference><notes></notes><tags></tags>"
		"</IssueTrackerCase>",
//TODO: xml-encode, check UTF-8 correctness etc:
		summary.c_str(),
		description.c_str()
	);
	string url = concat_path_file(m_sStrataURL.c_str(), "cases");

	int http_resp_code;
	char *res = post(&http_resp_code, url.c_str(), postdata.c_str());
	string result = res ? res : "";
	free(res);
	if (http_resp_code / 100 != 2) { /* not 2xx */
		throw CABRTException(EXCEP_PLUGIN, _("server returned HTTP code %u, error message: %s"),
				http_resp_code, res ? result.c_str() : "(none)");
	}

	return result;
}

void CReporterRHticket::SetSettings(const map_plugin_settings_t& pSettings)
{
	m_pSettings = pSettings;

	map_plugin_settings_t::const_iterator end = pSettings.end();
	map_plugin_settings_t::const_iterator it;
	it = pSettings.find("URL");
	if (it != end) {
		m_sStrataURL = it->second;
	}
	it = pSettings.find("Login");
	if (it != end) {
		m_sLogin = it->second;
	}
	it = pSettings.find("Password");
	if (it != end) {
		m_sPassword = it->second;
	}
	it = pSettings.find("SSLVerify");
	if (it != end) {
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
