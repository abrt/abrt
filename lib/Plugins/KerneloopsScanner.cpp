#include "KerneloopsScanner.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "PluginSettings.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <asm/unistd.h>


#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define FILENAME_KERNELOOPS  "kerneloops"

void CKerneloopsScanner::WriteSysLog(int m_nCount)
{
    if (m_nCount > 0) {
        openlog("abrt", 0, LOG_KERN);
        syslog(LOG_WARNING, "Kerneloops: Reported %i kernel oopses to Abrt", m_nCount);
        closelog();
    }
}

void CKerneloopsScanner::Run(const std::string& pActionDir,
                             const std::string& pArgs)
{
    ScanDmesg();
    if (!m_bSysLogFileScanned)
    {
        ScanSysLogFile(m_sSysLogFile.c_str(), 1);
        m_bSysLogFileScanned = true;
    }
}

void CKerneloopsScanner::SaveOopsToDebugDump()
{
    comm_layer_inner_status("Creating kernel oops crash reports...");

    CDebugDump m_pDebugDump;
    char m_sPath[PATH_MAX];
    std::list<COops> m_pOopsList;

    time_t t = time(NULL);
    if (((time_t) -1) == t)
    {
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerKerneloops::Report(): cannot get local time.");
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


void CKerneloopsScanner::ScanDmesg()
{
    comm_layer_inner_debug("Scanning dmesg...");

    int m_nFoundOopses;
    char *buffer;

    buffer = (char*)calloc(getpagesize()+1, 1);

    syscall(__NR_syslog, 3, buffer, getpagesize());
    m_nFoundOopses = m_pSysLog.ExtractOops(buffer, strlen(buffer), 0);
    free(buffer);

    if (m_nFoundOopses > 0)
        SaveOopsToDebugDump();
}

void CKerneloopsScanner::ScanSysLogFile(const char *filename, int issyslog)
{
    comm_layer_inner_debug("Scanning syslog...");

    char *buffer;
    struct stat statb;
    FILE *file;
    int ret;
    int m_nFoundOopses;
    size_t buflen, nread;

    memset(&statb, 0, sizeof(statb));

    ret = stat(filename, &statb);

    if (statb.st_size < 1 || ret != 0)
        return;

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
    buflen = MIN(statb.st_size+1024, 32*1024*1024);
    buffer = (char*)calloc(buflen, 1);
    assert(buffer != NULL);

    file = fopen(filename, "rm");
    if (!file) {
        free(buffer);
        return;
    }
    fseek(file, -buflen, SEEK_END);
    nread = fread(buffer, 1, buflen, file);
    fclose(file);

    if (nread > 0)
        m_nFoundOopses = m_pSysLog.ExtractOops(buffer, nread, issyslog);
    free(buffer);

    if (m_nFoundOopses > 0) {
        SaveOopsToDebugDump();
        WriteSysLog(m_nFoundOopses);
    }
}

void CKerneloopsScanner::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    plugin_load_settings(pPath, settings);

    if (settings.find("SysLogFile")!= settings.end())
    {
        m_sSysLogFile = settings["SysLogFile"];
    }
}
