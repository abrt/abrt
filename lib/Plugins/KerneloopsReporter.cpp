/*
 * Copyright 2007, Intel Corporation
 * Copyright 2009, Red Hat Inc.
 *
 * This file is part of %TBD%
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

#include "DebugDump.h"
#include "Settings.h"
#include "KerneloopsReporter.h"

#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <curl/curl.h>

CKerneloopsReporter::CKerneloopsReporter() :
    m_sSubmitURL("http://submit.kerneloops.org/submitoops.php")
{}

size_t writefunction(void *ptr, size_t size, size_t nmemb, void __attribute((unused)) *stream)
{
	char *c, *c1, *c2;

	c = (char*)malloc(size*nmemb + 1);
	memset(c, 0, size*nmemb + 1);
	memcpy(c, ptr, size*nmemb);
	printf("received %s \n", c);
	c1 = strstr(c, "201 ");
	if (c1) {
		c1+=4;
		c2 = strchr(c1, '\n');
		if (c2)
			*c2 = 0;
	}

	return size * nmemb;
}

void CKerneloopsReporter::Report(const crash_report_t& pReport)
{
	CURL *handle;
	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;

	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, m_sSubmitURL.c_str());

	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "oopsdata",
		CURLFORM_COPYCONTENTS, pReport.m_sTextData1.c_str(),
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "pass_on_allowed",
		CURLFORM_COPYCONTENTS, "yes",
		CURLFORM_END);

	curl_easy_setopt(handle, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunction);

	curl_easy_perform(handle);

	curl_formfree(post);
	curl_easy_cleanup(handle);
}

void CKerneloopsReporter::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    load_settings(pPath, settings);

	if (settings.find("SubmitURL") != settings.end())
	{
		m_sSubmitURL = settings["SubmitURL"];
	}
}
