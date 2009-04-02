#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include <string>
#include <map>
#include <vector>

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

// text value, should be displayed
#define TYPE_TXT "t"
// binary value, should be displayed
#define TYPE_BIN "b"
// system value, should not be displayed
#define TYPE_SYS "s"

typedef struct CCrashFile
{
    std::string m_sType;
    std::string m_sContent;
} crash_file_t;

// < key, type, value, key, type, value, ....>
typedef std::vector<std::string> vector_strings_t;
typedef std::map<std::string, crash_file_t> crash_report_t;

inline vector_strings_t crash_report_to_vector_strings(const crash_report_t& pCrashReport)
{
    vector_strings_t vec;
    crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        vec.push_back(it->first);
        vec.push_back(it->second.m_sType);
        vec.push_back(it->second.m_sContent);
    }
    return vec;
}

inline crash_report_t vector_strings_to_crash_report(const vector_strings_t& pVectorStrings)
{
    unsigned int ii;
    crash_report_t crashReport;
    for (ii = 0; ii < pVectorStrings.size(); ii += 3)
    {
        crash_file_t crashFile;
        std::string fileName = pVectorStrings[ii];
        crashFile.m_sType = pVectorStrings[ii + 1];
        crashFile.m_sContent = pVectorStrings[ii + 2];
        crashReport[fileName] = crashFile;
    }
    return crashReport;
}

#endif /* CRASHTYPES_H_ */
