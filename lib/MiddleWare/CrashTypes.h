#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include <string>
#include <map>
#include <vector>

// SYS - system value, should not be displayed
// BIN - binary value, should be displayed
// TXT = text value, should be displayed
#define CD_SYS          "s"
#define CD_BIN          "b"
#define CD_TXT          "t"

#define CD_ISEDITABLE       "y"
#define CD_ISNOTEDITABLE    "n"

#define CD_TYPE         (0)
#define CD_EDITABLE     (1)
#define CD_CONTENT      (2)

#define CI_UUID         "UUID"
#define CI_UID          "UID"
#define CI_COUNT        "Count"
#define CI_EXECUTABLE   "Executable"
#define CI_PACKAGE      "Package"
#define CI_DESCRIPTION  "Description"
#define CI_TIME         "Time"
#define CI_REPORTED     "Reported"
#define CI_COMMENT      "Comment"
#define CI_MWANALYZER   "_MWAnalyzer"
#define CI_MWUID        "_MWUID"
#define CI_MWUUID       "_MWUUID"

// now, size of a vecor is always 3 -> <type, editable, content>
typedef std::vector<std::string> vector_strings_t;
// <key, data>
typedef std::map<std::string, vector_strings_t> map_crash_data_t;

typedef map_crash_data_t map_crash_info_t;
typedef std::vector<map_crash_info_t> vector_crash_infos_t;
typedef map_crash_data_t map_crash_report_t;

inline void add_crash_data_to_crash_info(map_crash_info_t& pCrashInfo,
                                         const std::string& pItem,
                                         const std::string& pType,
                                         const std::string& pContent)
{
    pCrashInfo[pItem].push_back(pType);
    pCrashInfo[pItem].push_back(CD_ISNOTEDITABLE);
    pCrashInfo[pItem].push_back(pContent);
}

inline void add_crash_data_to_crash_report(map_crash_report_t& pCrashReport,
                                           const std::string& pItem,
                                           const std::string& pType,
                                           const std::string& pEditable,
                                           const std::string& pContent)
{
    pCrashReport[pItem].push_back(pType);
    pCrashReport[pItem].push_back(pEditable);
    pCrashReport[pItem].push_back(pContent);
}


#endif /* CRASHTYPES_H_ */
