#include "abrtlib.h"

#include "KerneloopsScanner.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#include <assert.h>
#include <syslog.h>
#include <asm/unistd.h>


#define FILENAME_KERNELOOPS  "kerneloops"

void CKerneloopsScanner::Run(const std::string& pActionDir,
			     const std::string& pArgs)
{
	int cnt_FoundOopses;

	/* Scan syslog file, on first call only */
	if (!m_bSysLogFileScanned)
	{
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
		m_bSysLogFileScanned = true;
	}

	/* Scan kernel's log buffer */
	cnt_FoundOopses = ScanDmesg();
	if (cnt_FoundOopses > 0)
		SaveOopsToDebugDump();
}

void CKerneloopsScanner::SaveOopsToDebugDump()
{
	comm_layer_inner_status("Creating kernel oops crash reports...");

	time_t t = time(NULL);
	CDebugDump debugDump;
	std::list<COops> oopsList = m_pSysLog.GetOopsList();
	m_pSysLog.ClearOopsList();

	while (!oopsList.empty()) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/kerneloops-%lu-%lu",
			DEBUG_DUMPS_DIR, (long)t, (long)oopsList.size());

		COops oops = oopsList.back();

		try
		{
			debugDump.Create(path, 0);
			debugDump.SaveText(FILENAME_ANALYZER, "Kerneloops");
			debugDump.SaveText(FILENAME_EXECUTABLE, "kernel");
			debugDump.SaveText(FILENAME_KERNEL, oops.m_sVersion);
			debugDump.SaveText(FILENAME_PACKAGE, "not_applicable");
			debugDump.SaveText(FILENAME_KERNELOOPS, oops.m_sData);
			debugDump.Close();
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
	comm_layer_inner_debug("Scanning dmesg...");

	int cnt_FoundOopses;
	char *buffer;
	int pagesz = getpagesize();

	buffer = (char*)xzalloc(pagesz + 1);

	syscall(__NR_syslog, 3, buffer, pagesz);
	cnt_FoundOopses = m_pSysLog.ExtractOops(buffer, strlen(buffer));
	free(buffer);

	return cnt_FoundOopses;
}

int CKerneloopsScanner::ScanSysLogFile(const char *filename)
{
	comm_layer_inner_debug("Scanning syslog...");

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
	if (sz > 0)
		cnt_FoundOopses = m_pSysLog.ExtractOops(buffer, sz);
	free(buffer);

	return cnt_FoundOopses;
}

void CKerneloopsScanner::LoadSettings(const std::string& pPath)
{
	map_plugin_settings_t settings;
	plugin_load_settings(pPath, settings);

	SetSettings(settings);
}

void CKerneloopsScanner::SetSettings(const map_plugin_settings_t& pSettings)
{
	if (pSettings.find("SysLogFile") != pSettings.end())
	{
		m_sSysLogFile = pSettings.find("SysLogFile")->second;
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

/* for dumpoops tool */
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
