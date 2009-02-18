#include "Logger.h"
#include <fstream>

CLogger::CLogger() :
    m_sLogPath("/tmp/CCLogger"),
    m_bAppendLogs(true)
{}

void CLogger::SetSettings(const map_settings_t& pSettings)
{
    if (pSettings.find("Log_Path")!= pSettings.end())
    {
        m_sLogPath = pSettings.find("Log_Path")->second;
    }
    if (pSettings.find("Append_Logs")!= pSettings.end())
    {
        m_bAppendLogs = pSettings.find("Append_Logs")->second == "yes";
    }
}

void CLogger::Report(const crash_report_t& pReport)
{
    std::ofstream fOut;
    if (m_bAppendLogs)
    {
        fOut.open(m_sLogPath.c_str(), std::ios::app);
    }
    else
    {
        fOut.open(m_sLogPath.c_str());
    }
    if (fOut.is_open())
    {
         fOut << "Common information" << std::endl;
         fOut << "==================" << std::endl << std::endl;
         fOut << "Architecture" << std::endl;
         fOut << "------------" << std::endl;
         fOut << pReport.m_sArchitecture << std::endl << std::endl;
         fOut << "Kernel version" << std::endl;
         fOut << "--------------" << std::endl;
         fOut << pReport.m_sKernel << std::endl << std::endl;
         fOut << "Package" << std::endl;
         fOut << "-------" << std::endl;
         fOut << pReport.m_sPackage << std::endl << std::endl;
         fOut << "Executable" << std::endl;
         fOut << "----------" << std::endl;
         fOut << pReport.m_sExecutable << std::endl << std::endl;
         fOut << "CmdLine" << std::endl;
         fOut << "----------" << std::endl;
         fOut << pReport.m_sCmdLine << std::endl << std::endl;
         fOut << "Created report" << std::endl;
         fOut << "==============" << std::endl;
         fOut << "Text reports" << std::endl;
         fOut << "==============" << std::endl;
         if (pReport.m_sTextData1 != "")
         {
             fOut << "Text Data 1" << std::endl;
             fOut << "-----------" << std::endl;
             fOut << pReport.m_sTextData1 << std::endl << std::endl;
         }
         if (pReport.m_sTextData2 != "")
         {
             fOut << "Text Data 2" << std::endl;
             fOut << "-----------" << std::endl;
             fOut << pReport.m_sTextData2 << std::endl << std::endl;
         }
         fOut << "Binary reports" << std::endl;
         fOut << "==============" << std::endl;
         if (pReport.m_sBinaryData1 != "")
         {
             fOut << "1. " <<  pReport.m_sBinaryData1 << std::endl;
         }
         if (pReport.m_sBinaryData2 != "")
         {
             fOut << "2. " <<  pReport.m_sBinaryData2 << std::endl;
         }
         fOut << std::endl;
         fOut.close();
    }
}
