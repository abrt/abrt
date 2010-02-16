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

    Authors:
       Anton Arapov <anton@redhat.com>
       Arjan van de Ven <arjan@linux.intel.com>
 */

#include "abrtlib.h"
#include "Kerneloops.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

std::string CAnalyzerKerneloops::GetLocalUUID(const char *pDebugDumpDir)
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

std::string CAnalyzerKerneloops::GetGlobalUUID(const char *pDebugDumpDir)
{
	return GetLocalUUID(pDebugDumpDir);
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerKerneloops,
            "Kerneloops",
            "0.0.2",
            "Analyzes kernel oopses",
            "anton@redhat.com",
            "https://people.redhat.com/aarapov",
            "");
