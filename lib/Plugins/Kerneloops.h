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
#include "Application.h"

#include <string>

class CApplicationKerneloops : public CApplication
{
	public:
		CApplicationKerneloops() {}
		virtual ~CApplicationKerneloops() {}
		std::string GetLocalUUID(const std::string& pDebugDumpDir);
		std::string GetGlobalUUID(const std::string& pDebugDumpDir);
		void CreateReport(const std::string& pDebugDumpDir) {}
		void Init();
		void DeInit() {}
		void SetSettings(const map_settings_t& pSettings) {}
};

PLUGIN_INFO(APPLICATION,
			"Kerneloops",
			"0.0.1",
			"Abrt's Kerneloops plugin.",
			"anton@redhat.com",
			"https://people.redhat.com/aarapov");

PLUGIN_INIT(CApplicationKerneloops);

#endif
