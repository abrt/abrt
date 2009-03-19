/*
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
 */

#ifndef __INCLUDE_GUARD_KERNELOOPS_H_
#define __INCLUDE_GUARD_KERNELOOPS_H_

#include "Plugin.h"
#include "Analyzer.h"

#include <string>

class CAnalyzerKerneloops : public CAnalyzer
{
	public:
		virtual ~CAnalyzerKerneloops() {}
		std::string GetLocalUUID(const std::string& pDebugDumpDir);
		std::string GetGlobalUUID(const std::string& pDebugDumpDir);
		void CreateReport(const std::string& pDebugDumpDir) {}
		void Init();
};

PLUGIN_INFO(ANALYZER,
            CAnalyzerKerneloops,
			"Kerneloops",
			"0.0.1",
			"Abrt's Kerneloops plugin.",
			"anton@redhat.com",
			"https://people.redhat.com/aarapov");

#endif
