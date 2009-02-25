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
    std::string m_sDescription;
    std::string m_sTime;
    std::string m_sReported;

    const map_crash_t GetMap()
    {
        map_crash_t mci;
        mci["UUID"] = m_sUUID;
        mci["UID"] = m_sUID;
        mci["Count"] = m_sCount;
        mci["Executable"] = m_sExecutable;
        mci["Package"] = m_sPackage;
        mci["Description"] = m_sDescription;
        mci["Time"] = m_sTime;
        mci["Reported"] =  m_sReported;

        return mci;
    }
} crash_info_t;

typedef std::vector<crash_info_t> vector_crash_infos_t;

typedef struct SCrashReport
{
    std::string m_sMWID;
    std::string m_sUUID;
    std::string m_sArchitecture;
    std::string m_sKernel;
    std::string m_sRelease;
    std::string m_sExecutable;
    std::string m_sCmdLine;
    std::string m_sPackage;
    std::string m_sTextData1;
    std::string m_sTextData2;
    std::string m_sBinaryData1;
    std::string m_sBinaryData2;
    std::string m_sComment;

    const map_crash_t GetMap()
    {
        map_crash_t mci;
        mci["MWID"] = m_sMWID;
        mci["UUID"] = m_sUUID;
        mci["Architecture"] = m_sArchitecture;
        mci["Kernel"] = m_sKernel;
        mci["Release"] = m_sRelease;
        mci["Executable"] = m_sExecutable;
        mci["CmdLine"] = m_sCmdLine;
        mci["Package"] = m_sPackage;
        mci["TextData1"] = m_sTextData1;
        mci["TextData2"] = m_sTextData2;
        mci["BinaryData1"] = m_sBinaryData1;
        mci["BinaryData2"] = m_sBinaryData2;
        mci["Comment"] = m_sComment;

        return mci;
    }
    void SetFromMap(const map_crash_t& pMcr)
    {
        m_sMWID = pMcr.find("MWID")->second;
        m_sUUID = pMcr.find("UUID")->second;
        m_sArchitecture = pMcr.find("Architecture")->second;
        m_sKernel = pMcr.find("Kernel")->second;
        m_sRelease = pMcr.find("Release")->second;
        m_sExecutable = pMcr.find("Executable")->second;
        m_sCmdLine = pMcr.find("CmdLine")->second;
        m_sPackage = pMcr.find("Package")->second;
        m_sTextData1 = pMcr.find("TextData1")->second;
        m_sTextData2 = pMcr.find("TextData2")->second;
        m_sBinaryData1 = pMcr.find("BinaryData1")->second;
        m_sBinaryData2 = pMcr.find("BinaryData2")->second;
        m_sComment = pMcr.find("Comment")->second;
    }
} crash_report_t;

#endif /* CRASHTYPES_H_ */
