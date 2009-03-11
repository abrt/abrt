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

#ifndef __INCLUDE_GUARD_KERNELOOPSREPORTER_H_
#define __INCLUDE_GUARD_KERNELOOPSREPORTER_H_

#include <string>
#include "Plugin.h"
#include "Reporter.h"

class CKerneloopsReporter : public CReporter
{
	private:
		std::string m_sSubmitURL;

	public:
		CKerneloopsReporter();
		virtual ~CKerneloopsReporter() {}
		void Init() {}
		void DeInit() {}
		void SetSettings(const map_settings_t& pSettings);
		void Report(const crash_report_t& pReport);
};

PLUGIN_INFO(REPORTER,
		"KerneloopsReporter",
		"0.0.1",
		"Sends the Kerneloops crash information to Kerneloopsoops.org",
		"anton@redhat.com",
		"http://people.redhat.com/aarapov");

PLUGIN_INIT(CKerneloopsReporter);

#endif
