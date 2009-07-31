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

#include "KerneloopsReporter.h"
#include "PluginSettings.h"
#include "CommLayerInner.h"

//#include <stdlib.h>
//#include <string.h>
#include <curl/curl.h>

#define FILENAME_KERNELOOPS "kerneloops"

/* helpers */

static size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	char *c, *c1, *c2;

	size *= nmemb;
/*	log("received: '%*.*s'\n", (int)size, (int)size, (char*)ptr);
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

	handle = curl_easy_init();
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

void CKerneloopsReporter::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
	comm_layer_inner_status("Creating and submitting a report...");

	/* TODO: react on errorcode (!0 -> error) */
	http_post_to_kerneloops_site(
		m_sSubmitURL.c_str(),
		pCrashReport.find(FILENAME_KERNELOOPS)->second[CD_CONTENT].c_str()
	);
}

void CKerneloopsReporter::LoadSettings(const std::string& pPath)
{
	map_settings_t settings;
	plugin_load_settings(pPath, settings);

	if (settings.find("SubmitURL") != settings.end())
	{
		m_sSubmitURL = settings["SubmitURL"];
	}
}
