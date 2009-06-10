#ifndef SOCKETCRASHTYPES_H_
#define SOCKETCRASHTYPES_H_

#include "CrashTypes.h"
#include <sstream>
#include <stdlib.h>


#define MESSAGE_DELETE_DEBUG_DUMP   "(DELETE_DEBUG_DUMP)"
#define MESSAGE_GET_CRASH_INFOS     "(GET_CRASH_INFOS)"
#define MESSAGE_REPORT              "(REPORT)"
#define MESSAGE_CREATE_REPORT       "(CREATE_REPORT)"
#define MESSAGE_END_MARKER          23

inline std::string crash_data_to_string(const map_crash_data_t& pCrashData)
{
    std::stringstream sCD;
    map_crash_data_t::const_iterator it_cd;
    sCD << "(" << pCrashData.size() << ")";
    for(it_cd = pCrashData.begin(); it_cd != pCrashData.end(); it_cd++)
    {
        sCD << "(" << it_cd->first.length() << ")";
        sCD << it_cd->first;
        sCD << "(" << it_cd->second[CD_TYPE].length() << ")";
        sCD << it_cd->second[CD_TYPE];
        sCD << "(" << it_cd->second[CD_EDITABLE].length() << ")";
        sCD << it_cd->second[CD_EDITABLE];
        sCD << "(" << it_cd->second[CD_CONTENT].length() << ")";
        sCD << it_cd->second[CD_CONTENT];
    }
    return sCD.str();
}

inline std::string crash_infos_to_string(const vector_crash_infos_t& pCrashInfos)
{
    std::stringstream sCI;
    unsigned int ii;
    for (ii = 0; ii < pCrashInfos.size(); ii++)
    {
        sCI << crash_data_to_string(pCrashInfos[ii]);
    }
    return sCI.str();
}

inline std::string crash_report_to_string(const map_crash_report_t& pCrashReport)
{
    return crash_data_to_string(pCrashReport);
}

inline int get_number_from_string(const std::string& pMessage, int& len)
{
    std::string sNumber = "";

    int ii = 1;
    while (pMessage[ii] != ')')
    {
        sNumber += pMessage[ii];
        ii++;
        if (static_cast<std::string::size_type>(ii) >= pMessage.length())
        {
            len = ii;
            return -1;
        }
    }
    len = ii + 1;
    return atoi(sNumber.c_str());
}

inline map_crash_data_t string_to_crash_data(const std::string& pMessage, int& len)
{
    map_crash_data_t ci;
    std::string message = pMessage;
    int nSize;
    std::string sField;
    int nField;
    int nCount;
    std::string name;
    int ii;

    nCount = get_number_from_string(message, ii);
    if (ii == -1)
    {
        len = ii;
        return ci;
    }
    message.erase(0, ii);
    len = ii;
    nField = 0;
    while (nField < nCount * 4)
    {
        nSize = get_number_from_string(message, ii);
        if (ii == -1)
        {
            len += ii;
            ci.clear();
            return ci;
        }
        sField = message.substr(ii, nSize);
        message.erase(0, ii + nSize);
        len += ii + nSize;
        switch(nField % 4)
        {
            case 0:
                name = sField;
                break;
            default:
                ci[name].push_back(sField);
                break;
        }
        nField++;
    }
    return ci;
}

inline vector_crash_infos_t string_to_crash_infos(const std::string& pMessage)
{
    vector_crash_infos_t vci;
    std::string message = pMessage;
    int len;

    while (message != "")
    {
        map_crash_info_t crash_info = string_to_crash_data(message, len);
        if (crash_info.size() == 0)
        {
            return vci;
        }
        vci.push_back(crash_info);
        message.erase(0, len);
    }
    return vci;
}

inline map_crash_report_t string_to_crash_report(const std::string& pMessage)
{
    int len;

    map_crash_report_t crash_report = string_to_crash_data(pMessage, len);
    return crash_report;
}

#endif /* SOCKETCRASHTYPES_H_ */
