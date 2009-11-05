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
#include "Kerneloops.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#define FILENAME_KERNELOOPS  "kerneloops"

std::string CAnalyzerKerneloops::GetLocalUUID(const std::string& pDebugDumpDir)
{
	log(_("Getting local universal unique identification"));

	std::string oops;
	{
		CDebugDump dd;
		dd.Open(pDebugDumpDir);
		dd.LoadText(FILENAME_KERNELOOPS, oops);
	}

	/* An algorithm proposed by Donald E. Knuth in The Art Of Computer
	 * Programming Volume 3, under the topic of sorting and search
	 * chapter 6.4.
	 */
	unsigned len = oops.length();
	unsigned hash = len;
	for (unsigned i = 0; i < len; i++)
	{
		hash = ((hash << 5) ^ (hash >> 27)) ^ oops[i];
	}
	hash &= 0x7FFFFFFF;

	return to_string(hash);
}

std::string CAnalyzerKerneloops::GetGlobalUUID(const std::string& pDebugDumpDir)
{
	return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerKerneloops::SetSettings(const map_plugin_settings_t& pSettings)
{
	m_pSettings = pSettings;
}

map_plugin_settings_t CAnalyzerKerneloops::GetSettings()
{
	return m_pSettings;
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerKerneloops,
            "Kerneloops",
            "0.0.2",
            "Abrt's Kerneloops plugin.",
            "anton@redhat.com",
            "https://people.redhat.com/aarapov",
            "");
