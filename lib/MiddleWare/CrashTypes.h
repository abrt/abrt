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

#define CD_UUID         "UUID"
#define CD_UID          "UID"
#define CD_COUNT        "Count"
#define CD_EXECUTABLE   "Executable"
#define CD_PACKAGE      "Package"
#define CD_DESCRIPTION  "Description"
#define CD_TIME         "Time"
#define CD_REPORTED     "Reported"
#define CD_COMMENT      "Comment"
#define CD_REPRODUCE    "How to reproduce"
#define CD_MWANALYZER   "_MWAnalyzer"
#define CD_MWUID        "_MWUID"
#define CD_MWUUID       "_MWUUID"

// now, size of a vecor is always 3 -> <type, editable, content>
typedef std::vector<std::string> vector_strings_t;
// <key, data>
typedef std::map<std::string, vector_strings_t> map_crash_data_t;

typedef map_crash_data_t map_crash_info_t;
typedef std::vector<map_crash_info_t> vector_crash_infos_t;
typedef map_crash_data_t map_crash_report_t;

inline void add_crash_data_to_crash_info(map_crash_info_t& pCrashInfo,
                                         const std::string& pItem,
                                         const std::string& pContent)
{
    pCrashInfo[pItem].push_back(CD_TXT);
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
