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

#include "Kerneloops.h"
#include "KerneloopsSysLog.h"
#include "DebugDump.h"
#include "Settings.h"

#include <sstream>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <asm/unistd.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))

CAnalyzerKerneloops::CAnalyzerKerneloops() :
	m_sSysLogFile("/var/log/messages")
{}

void CAnalyzerKerneloops::WriteSysLog(int m_nCount)
{
	if (m_nCount > 0) {
		openlog("abrt", 0, LOG_KERN);
		syslog(LOG_WARNING, "Kerneloops: Reported %i kernel oopses to Abrt", m_nCount);
		closelog();
	}
}

std::string CAnalyzerKerneloops::GetLocalUUID(const std::string& pDebugDumpDir)
{
	std::string m_sOops;
	std::stringstream m_sHash;
	CDebugDump m_pDebugDump;
	m_pDebugDump.Open(pDebugDumpDir);
	m_pDebugDump.LoadText(FILENAME_TEXTDATA1, m_sOops);

	/* An algorithm proposed by Donald E. Knuth in The Art Of Computer
	 * Programming Volume 3, under the topic of sorting and search
	 * chapter 6.4.
	 */
	unsigned int m_nHash = static_cast<unsigned int>(m_sOops.length());
	for(std::size_t i = 0; i < m_sOops.length(); i++)
	{
		m_nHash = ((m_nHash << 5) ^ (m_nHash >> 27)) ^ m_sOops[i];
	}
	m_sHash << (m_nHash & 0x7FFFFFFF);

	return m_sHash.str();
}

std::string CAnalyzerKerneloops::GetGlobalUUID(const std::string& pDebugDumpDir)
{
	return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerKerneloops::Report()
{
	CDebugDump m_pDebugDump;
	char m_sPath[PATH_MAX];
	std::list<COops> m_pOopsList;

	time_t t = time(NULL);
	if (((time_t) -1) == t)
	{
		fprintf(stderr, "Kerneloops: cannot get local time.\n");
		perror("");
		// TODO: throw -4
	}

	m_pOopsList = m_pSysLog.GetOopsList();
	m_pSysLog.ClearOopsList();
	while (!m_pOopsList.empty())
	{
		snprintf(m_sPath, sizeof(m_sPath), "%s/kerneloops-%d-%d", DEBUG_DUMPS_DIR, t, m_pOopsList.size());

		COops m_pOops;
		m_pOops = m_pOopsList.back();

		try
		{
			m_pDebugDump.Create(m_sPath);
			m_pDebugDump.SaveText(FILENAME_ANALYZER, "Kerneloops");
			m_pDebugDump.SaveText(FILENAME_UID, "0");
			m_pDebugDump.SaveText(FILENAME_EXECUTABLE, "kernel");
			m_pDebugDump.SaveText(FILENAME_KERNEL, m_pOops.m_sVersion);
			m_pDebugDump.SaveText(FILENAME_PACKAGE, "not_applicable");
			m_pDebugDump.SaveText(FILENAME_TEXTDATA1, m_pOops.m_sData);
			m_pDebugDump.Close();
		}
		catch (std::string sError)
		{
			fprintf(stderr, "Kerneloops: %s\n", sError.c_str());
			// TODO: throw -2
		}
		m_pOopsList.pop_back();
	}
}

void CAnalyzerKerneloops::Init()
{
	/* hack: release Init() */
	pid_t pid = fork();
	if (pid)
		return;
	// TODO: throw if we can't fork()

	sched_yield();

#ifdef PR_SET_TIMERSLACK
	/*
	 * Signal the kernel that we're not timing critical
	 */
	prctl(PR_SET_TIMERSLACK,1000*1000*1000, 0, 0, 0);
#endif

	/* we scan dmesg before /var/log/messages; dmesg is a more accurate source normally */
	ScanDmesg();
	/* during boot... don't go too fast and slow the system down */
	sleep(10);
	ScanSysLogFile(m_sSysLogFile.c_str(), 1);

	while(1) {
		sleep(10);
		ScanDmesg();
	}
}

void CAnalyzerKerneloops::ScanDmesg()
{
	int m_nFoundOopses;
	char *buffer;

	buffer = (char*)calloc(getpagesize()+1, 1);

	syscall(__NR_syslog, 3, buffer, getpagesize());
	m_nFoundOopses = m_pSysLog.ExtractOops(buffer, strlen(buffer), 0);
	free(buffer);

	if (m_nFoundOopses > 0)
		Report();
}

void CAnalyzerKerneloops::ScanSysLogFile(const char *filename, int issyslog)
{
	char *buffer;
	struct stat statb;
	FILE *file;
	int ret;
	int m_nFoundOopses;
	size_t buflen;

	memset(&statb, 0, sizeof(statb));

	ret = stat(filename, &statb);

	if (statb.st_size < 1 || ret != 0)
		return;

	/*
	 * in theory there's a race here, since someone could spew
	 * to /var/log/messages before we read it in... we try to
	 * deal with it by reading at most 1023 bytes extra. If there's
	 * more than that.. any oops will be in dmesg anyway.
	 * Do not try to allocate an absurt amount of memory; ignore
	 * older log messages because they are unlikely to have
	 * sufficiently recent data to be useful.  32MB is more
	 * than enough; it's not worth looping through more log
	 * if the log is larger than that.
	 */
	buflen = MAX(statb.st_size+1024, 32*1024*1024);
	buffer = (char*)calloc(buflen, 1);
	assert(buffer != NULL);

	file = fopen(filename, "rm");
	if (!file) {
		free(buffer);
		return;
	}
	fseek(file, -buflen, SEEK_END);
	ret = fread(buffer, 1, buflen-1, file);
	fclose(file);

	if (ret > 0)
		m_nFoundOopses = m_pSysLog.ExtractOops(buffer, buflen-1, issyslog);
	free(buffer);

	if (m_nFoundOopses > 0) {
		Report();
		WriteSysLog(m_nFoundOopses);
	}
}

void CAnalyzerKerneloops::LoadSettings(const std::string& pPath)
{
	map_settings_t settings;
	load_settings(pPath, settings);

	if (settings.find("SysLogFile")!= settings.end())
	{
		m_sSysLogFile = settings["SysLogFile"];
	}
}
