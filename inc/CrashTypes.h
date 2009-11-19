#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include "abrt_types.h"

// SYS - system value, should not be displayed
// BIN - binary value, should be displayed as a path to binary file
// TXT - text value, should be displayed
// ATT - text value which can be sent as attachment via reporters
// TODO: maybe we don't need separate CD_ATT - can simply look at strlen(content)
// in all places which want to handle "long" and "short" texts differently
#define CD_SYS          "s"
#define CD_BIN          "b"
#define CD_TXT          "t"
#define CD_ATT          "a"

#define CD_ATT_SIZE     (256)

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
#define CD_MESSAGE      "Message"
#define CD_COMMENT      "Comment"
#define CD_REPRODUCE    "How to reproduce"
#define CD_MWANALYZER   "_MWAnalyzer"
#define CD_MWUID        "_MWUID"
#define CD_MWUUID       "_MWUUID"
#define CD_MWDDD        "_MWDDD"

// currently, vector always has exactly 3 elements -> <type, editable, content>
// <key, data>
typedef map_vector_string_t map_crash_data_t;

typedef map_crash_data_t map_crash_info_t;
typedef map_crash_data_t map_crash_report_t;
typedef std::vector<map_crash_info_t> vector_crash_infos_t;

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
