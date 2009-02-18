#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include <string>
#include <map>

typedef std::map<std::string, std::string> map_crash_t;

typedef struct SCrashInfo
{
    std::string m_sUUID;
    std::string m_sUID;
    std::string m_sCount;
    std::string m_sExecutable;
    std::string m_sPackage;
    std::string m_sTime;

    const map_crash_t GetMap()
    {
        map_crash_t mci;
        mci["UUID"] = m_sUUID;
        mci["UID"] = m_sUID;
        mci["Count"] = m_sCount;
        mci["Executable"] = m_sExecutable;
        mci["Package"] = m_sPackage;
        mci["Time"] = m_sTime;

        return mci;
    }
} crash_info_t;

typedef std::vector<crash_info_t> vector_crash_infos_t;

typedef struct SCrashContex
{
    std::string m_sUUID;
    std::string m_sUID;
    std::string m_sLanAppPlugin;
} crash_context_t;


typedef struct SCrashReport
{
    std::string m_sArchitecture;
    std::string m_sKernel;
    std::string m_sExecutable;
    std::string m_sCmdLine;
    std::string m_sPackage;
    std::string m_sTextData1;
    std::string m_sTextData2;
    std::string m_sBinaryData1;
    std::string m_sBinaryData2;

    const map_crash_t GetMap()
    {
        map_crash_t mci;
        mci["Architecture"] = m_sArchitecture;
        mci["Kernel"] = m_sKernel;
        mci["Executable"] = m_sExecutable;
        mci["CmdLine"] = m_sCmdLine;
        mci["Package"] = m_sPackage;
        mci["TextData1"] = m_sTextData1;
        mci["TextData2"] = m_sTextData2;
        mci["BinaryData1"] = m_sBinaryData1;
        mci["BinaryData2"] = m_sBinaryData2;

        return mci;
    }
} crash_report_t;

#endif /* CRASHTYPES_H_ */
