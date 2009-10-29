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

#include <assert.h>
#include <syslog.h>
#include <asm/unistd.h> /* __NR_syslog */
#include "abrtlib.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "KerneloopsSysLog.h"
#include "KerneloopsScanner.h"

#include <limits.h>

#define FILENAME_KERNELOOPS  "kerneloops"

// TODO: https://fedorahosted.org/abrt/ticket/78

CKerneloopsScanner::CKerneloopsScanner()
{
	int cnt_FoundOopses;
	m_sSysLogFile = "/var/log/messages";

	/* Scan dmesg, on first call only */
	cnt_FoundOopses = ScanDmesg();
	if (cnt_FoundOopses > 0)
		SaveOopsToDebugDump();
}

void CKerneloopsScanner::Run(const std::string& pActionDir,
			     const std::string& pArgs)
{
	int cnt_FoundOopses;

	cnt_FoundOopses = ScanSysLogFile(m_sSysLogFile.c_str());
	if (cnt_FoundOopses > 0) {
		SaveOopsToDebugDump();
		/*
		 * This marker in syslog file prevents us from
		 * re-parsing old oopses (any oops before it is
		 * ignored by ScanSysLogFile()). The only problem
		 * is that we can't be sure here that m_sSysLogFile
		 * is the file where syslog(xxx) stuff ends up.
		 */
		openlog("abrt", 0, LOG_KERN);
		syslog(
			LOG_WARNING,
			"Kerneloops: Reported %u kernel oopses to Abrt",
			cnt_FoundOopses
		);
		closelog();
	}
}

void CKerneloopsScanner::SaveOopsToDebugDump()
{
	update_client(_("Creating kernel oops crash reports..."));

	int countdown = 16; /* do not report hundreds of oopses */
	time_t t = time(NULL);
	vector_string_t oopsList = m_pOopsList;
	m_pOopsList.clear();

	while (!oopsList.empty() && --countdown != 0) {
		char path[sizeof(DEBUG_DUMPS_DIR"/kerneloops-%lu-%lu") + 2 * sizeof(long)*3];
		sprintf(path, DEBUG_DUMPS_DIR"/kerneloops-%lu-%lu",
				(long)t, (long)oopsList.size());

		std::string oops = oopsList.back();
		const char *first_line = oops.c_str();
		char *second_line = (char*)strchr(first_line, '\n'); /* never NULL */
              	*second_line++ = '\0';
		try
		{
			CDebugDump debugDump;
			debugDump.Create(path, UINT_MAX);
			debugDump.SaveText(FILENAME_ANALYZER, "Kerneloops");
			debugDump.SaveText(FILENAME_EXECUTABLE, "kernel");
			debugDump.SaveText(FILENAME_KERNEL, first_line);
			debugDump.SaveText(FILENAME_PACKAGE, "not_applicable");
			debugDump.SaveText(FILENAME_KERNELOOPS, second_line);
		}
		catch (CABRTException& e)
		{
			throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": " + e.what());
		}
		oopsList.pop_back();
	}
}

int CKerneloopsScanner::ScanDmesg()
{
	VERB1 log("Scanning dmesg...");

	int cnt_FoundOopses;
	char *buffer;
	int pagesz = getpagesize();

	buffer = (char*)xzalloc(pagesz + 1);

	syscall(__NR_syslog, 3, buffer, pagesz);
	m_pOopsList.clear();
	cnt_FoundOopses = extract_oopses(m_pOopsList, buffer, strlen(buffer));
	free(buffer);

	return cnt_FoundOopses;
}

int CKerneloopsScanner::ScanSysLogFile(const char *filename)
{
	VERB1 log("Scanning syslog...");

	char *buffer;
	struct stat statb;
	int fd;
	int cnt_FoundOopses;
	ssize_t sz;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;
	statb.st_size = 0; /* paranoia */
	if (fstat(fd, &statb) != 0 || statb.st_size < 1)
		return 0;

	/*
	 * in theory there's a race here, since someone could spew
	 * to /var/log/messages before we read it in... we try to
	 * deal with it by reading at most 1023 bytes extra. If there's
	 * more than that.. any oops will be in dmesg anyway.
	 * Do not try to allocate an absurd amount of memory; ignore
	 * older log messages because they are unlikely to have
	 * sufficiently recent data to be useful.  32MB is more
	 * than enough; it's not worth looping through more log
	 * if the log is larger than that.
	 */
	sz = statb.st_size + 1024;
	if (statb.st_size > (32*1024*1024 - 1024)) {
		xlseek(fd, -(32*1024*1024 - 1024), SEEK_END);
		sz = 32*1024*1024;
	}
	buffer = (char*)xzalloc(sz);
	sz = full_read(fd, buffer, sz);
	close(fd);

	cnt_FoundOopses = 0;
	if (sz > 0) {
		m_pOopsList.clear();
		cnt_FoundOopses = extract_oopses(m_pOopsList, buffer, sz);
	}
	free(buffer);

	return cnt_FoundOopses;
}

void CKerneloopsScanner::SetSettings(const map_plugin_settings_t& pSettings)
{
	map_plugin_settings_t::const_iterator it = pSettings.find("SysLogFile");
	if (it != pSettings.end())
	{
		m_sSysLogFile = it->second;
	}
}

map_plugin_settings_t CKerneloopsScanner::GetSettings()
{
	map_plugin_settings_t ret;

	ret["SysLogFile"] = m_sSysLogFile;

	return ret;
}

PLUGIN_INFO(ACTION,
            CKerneloopsScanner,
            "KerneloopsScanner",
            "0.0.1",
            "Save new Kerneloops crashes into debug dump dir",
            "anton@redhat.com",
            "http://people.redhat.com/aarapov",
            "");

/* For "dumpoops" tool */
extern "C" {

int scan_syslog_file(CKerneloopsScanner *This, const char *filename)
{
	return This->ScanSysLogFile(filename);
}

void save_oops_to_debug_dump(CKerneloopsScanner *This)
{
	This->SaveOopsToDebugDump();
}

}
