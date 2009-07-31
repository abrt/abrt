#include "abrtlib.h"

#include "KerneloopsScanner.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "PluginSettings.h"

#include <assert.h>
//#include <stdlib.h>
//#include <string.h>
#include <syslog.h>
//#include <sys/stat.h>
#include <asm/unistd.h>


#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define FILENAME_KERNELOOPS  "kerneloops"

void CKerneloopsScanner::Run(const std::string& pActionDir,
                             const std::string& pArgs)
{
    int cnt_FoundOopses;

    if (!m_bSysLogFileScanned)
    {
        cnt_FoundOopses = ScanSysLogFile(m_sSysLogFile.c_str(), 1);
        if (cnt_FoundOopses > 0) {
            SaveOopsToDebugDump();
            openlog("abrt", 0, LOG_KERN);
            syslog(LOG_WARNING, "Kerneloops: Reported %u kernel oopses to Abrt", cnt_FoundOopses);
            closelog();
        }
        m_bSysLogFileScanned = true;
    }
    cnt_FoundOopses = ScanDmesg();
    if (cnt_FoundOopses > 0)
        SaveOopsToDebugDump();
}

void CKerneloopsScanner::SaveOopsToDebugDump()
{
    comm_layer_inner_status("Creating kernel oops crash reports...");

    CDebugDump m_pDebugDump;
    char m_sPath[PATH_MAX];
    std::list<COops> m_pOopsList;

    time_t t = time(NULL);

    m_pOopsList = m_pSysLog.GetOopsList();
    m_pSysLog.ClearOopsList();
    while (!m_pOopsList.empty())
    {
        snprintf(m_sPath, sizeof(m_sPath), "%s/kerneloops-%ld-%ld", DEBUG_DUMPS_DIR, (long)t, (long)m_pOopsList.size());

        COops m_pOops;
        m_pOops = m_pOopsList.back();

        try
        {
            m_pDebugDump.Create(m_sPath, "0");
            m_pDebugDump.SaveText(FILENAME_ANALYZER, "Kerneloops");
            m_pDebugDump.SaveText(FILENAME_EXECUTABLE, "kernel");
            m_pDebugDump.SaveText(FILENAME_KERNEL, m_pOops.m_sVersion);
            m_pDebugDump.SaveText(FILENAME_PACKAGE, "not_applicable");
            m_pDebugDump.SaveText(FILENAME_KERNELOOPS, m_pOops.m_sData);
            m_pDebugDump.Close();
        }
        catch (CABRTException& e)
        {
            throw CABRTException(EXCEP_PLUGIN, "CAnalyzerKerneloops::Report(): " + e.what());
        }
        m_pOopsList.pop_back();
    }
}


int CKerneloopsScanner::ScanDmesg()
{
    comm_layer_inner_debug("Scanning dmesg...");

    int cnt_FoundOopses;
    char *buffer;

    buffer = (char*)xzalloc(getpagesize()+1);

    syscall(__NR_syslog, 3, buffer, getpagesize());
    cnt_FoundOopses = m_pSysLog.ExtractOops(buffer, strlen(buffer), 0);
    free(buffer);

    return cnt_FoundOopses;
}

int CKerneloopsScanner::ScanSysLogFile(const char *filename, int issyslog)
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
        cnt_FoundOopses = m_pSysLog.ExtractOops(buffer, sz, issyslog);
    free(buffer);

    return cnt_FoundOopses;
}

void CKerneloopsScanner::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    plugin_load_settings(pPath, settings);

    if (settings.find("SysLogFile") != settings.end())
    {
        m_sSysLogFile = settings["SysLogFile"];
    }
}
