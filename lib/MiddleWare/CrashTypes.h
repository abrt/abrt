#ifndef CRASHTYPES_H_
#define CRASHTYPES_H_

#include <string>
#include <map>
#include <vector>

// SYS - system value, should not be displayed
// BIN - binary value, should be displayed
// TXT = text value, should be displayed
typedef enum { CD_SYS, CD_BIN, CD_TXT } content_crash_data_t;

const char* const type_crash_data_t_str[] = { "s", "b", "t" };

typedef enum { CI_UUID,
               CI_UID,
               CI_COUNT,
               CI_EXECUTABLE,
               CI_PACKAGE,
               CI_DESCRIPTION,
               CI_TIME,
               CI_REPORTED,
               CI_MWANALYZER,
               CI_MWUID,
               CI_MWUUID } item_crash_into_t;

const char* const item_crash_into_t_str[] = { "UUID",
                                              "UID",
                                              "Count",
                                              "Executable",
                                              "Package",
                                              "Time",
                                              "Reported",
                                              "_MWAnalyzer",
                                              "_MWUID",
                                              "_MWUUID" };

typedef enum { CD_TYPE, CD_CONTENT } item_crash_data_t;

// now, size of a vecor is always 2 -> <type, content>
typedef std::vector<std::string> vector_strings_t;
// <key, data>
typedef std::map<std::string, vector_strings_t> map_crash_data_t;

typedef map_crash_data_t map_crash_info_t;
typedef std::vector<map_crash_info_t> vector_crash_infos_t;
typedef map_crash_data_t map_crash_report_t;

inline void add_crash_data_to_crash_info(map_crash_info_t& pCrashInfo,
                                         const item_crash_into_t& pItem,
                                         const content_crash_data_t& pType,
                                         const std::string& pContent)
{
    pCrashInfo[item_crash_into_t_str[pItem]].push_back(type_crash_data_t_str[pType]);
    pCrashInfo[item_crash_into_t_str[pItem]].push_back(pContent);
}

inline void add_crash_data_to_crash_report(map_crash_report_t& pCrashReport,
                                           const std::string& pFileName,
                                           const content_crash_data_t& pType,
                                           const std::string& pContent)
{
    pCrashReport[pFileName].push_back(type_crash_data_t_str[pType]);
    pCrashReport[pFileName].push_back(pContent);
}


#endif /* CRASHTYPES_H_ */
