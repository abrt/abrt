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

#ifndef __INCLUDE_GUARD_KERNELOOPSSYSLOG_H_
#define __INCLUDE_GUARD_KERNELOOPSSYSLOG_H_

#include <string>
#include <list>

class COops
{
	public:
		std::string m_sData;
		std::string m_sVersion;
};

class CSysLog
{
	private:
		void QueueOops(char *data, char *version);
		int ExtractVersion(char *linepointer, char *version);
		int FillLinePointers(char *buffer, size_t buflen);
		std::list<COops> m_OopsQueue;
		int m_nFoundOopses;

	public:
		CSysLog();
		const std::list<COops>& GetOopsList();
		void ClearOopsList();
		int ExtractOops(char *buffer, size_t buflen);
};

#endif
