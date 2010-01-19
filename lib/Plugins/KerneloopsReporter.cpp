/*
 * Copyright 2007, Intel Corporation
 * Copyright 2009, Red Hat Inc.
 *
 * This file is part of Abrt.
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authors:
 *      Anton Arapov <anton@redhat.com>
 *      Arjan van de Ven <arjan@linux.intel.com>
 */

#include "abrtlib.h"
#include "abrt_xmlrpc.h" /* for xcurl_easy_init */
#include "KerneloopsReporter.h"
#include "CommLayerInner.h"
#include "ABRTException.h"

#define FILENAME_KERNELOOPS "kerneloops"

/* helpers */
static size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size *= nmemb;
/*
	char *c, *c1, *c2;

	log("received: '%*.*s'\n", (int)size, (int)size, (char*)ptr);
	c = (char*)xzalloc(size + 1);
	memcpy(c, ptr, size);
	c1 = strstr(c, "201 ");
	if (c1) {
		c1 += 4;
		c2 = strchr(c1, '\n');
		if (c2)
			*c2 = 0;
	}
	free(c);
*/

	return size;
}

/* Send oops data to kerneloops.org-style site, using HTTP POST */
/* Returns 0 on success */
static int http_post_to_kerneloops_site(const char *url, const char *oopsdata)
{
	CURLcode ret;
	CURL *handle;
	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;

	handle = xcurl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, url);

	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "oopsdata",
		CURLFORM_COPYCONTENTS, oopsdata,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "pass_on_allowed",
		CURLFORM_COPYCONTENTS, "yes",
		CURLFORM_END);

	curl_easy_setopt(handle, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunction);

	ret = curl_easy_perform(handle);

	curl_formfree(post);
	curl_easy_cleanup(handle);

	return ret != 0;
}


/* class CKerneloopsReporter */
CKerneloopsReporter::CKerneloopsReporter() :
	m_sSubmitURL("http://submit.kerneloops.org/submitoops.php")
{}

std::string CKerneloopsReporter::Report(const map_crash_data_t& pCrashData,
                                        const map_plugin_settings_t& pSettings,
                                        const char *pArgs)
{
	int ret = -1;

	update_client(_("Creating and submitting a report..."));

	map_crash_data_t::const_iterator it = pCrashData.find(FILENAME_KERNELOOPS);
	if (it != pCrashData.end()) {
		ret = http_post_to_kerneloops_site(
			m_sSubmitURL.c_str(),
			it->second[CD_CONTENT].c_str()
		);
	}

	if (ret) {
		/* FIXME: be more informative */
		throw CABRTException(EXCEP_PLUGIN, "CKerneloopsReporter::Report(): Report has not been sent");
	}
        /* Server replies with:
         * 200 thank you for submitting the kernel oops information
         * RemoteIP: 34192fd15e34bf60fac6a5f01bba04ddbd3f0558
         * - no URL or bug ID apparently...
         */
	return "Kernel oops report was uploaded";
}

void CKerneloopsReporter::SetSettings(const map_plugin_settings_t& pSettings)
{
	m_pSettings = pSettings;

	map_plugin_settings_t::const_iterator end = pSettings.end();
	map_plugin_settings_t::const_iterator it;
	it = pSettings.find("SubmitURL");
	if (it != end) {
		m_sSubmitURL = it->second;
	}
}

//ok to delete?
//const map_plugin_settings_t& CKerneloopsReporter::GetSettings()
//{
//	m_pSettings["SubmitURL"] = m_sSubmitURL;
//
//	return m_pSettings;
//}

PLUGIN_INFO(REPORTER,
            CKerneloopsReporter,
            "KerneloopsReporter",
            "0.0.1",
            "Sends kernel oops information to kerneloops.org",
            "anton@redhat.com",
            "http://people.redhat.com/aarapov",
            PLUGINS_LIB_DIR"/KerneloopsReporter.GTKBuilder");
