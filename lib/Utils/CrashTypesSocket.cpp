/*
    CrashTypesSocket.cpp - functions for socket communication

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    */

#include "abrtlib.h"
#include "CrashTypesSocket.h"

/**
 * A request GET_CRASH_INFOS has the following form:
 * message -> MESSAGE_GET_CRASH_INFOS
 *
 * Example:
 *
 * (GET_CRASH_INFOS)
 * \23
 *
 *
 * A request DELETE_DEBUG_DUMP and CREATE_REPORT has following form:
 * message -> MESSAGE_TYPE data END_MARKER
 * MESSAGE_TYPE -> MESSAGE_CREATE_REPORT | MESSAGE_DELETE_DEBUG_DUMP
 * data -> UUID
 *
 * Example:
 * (DELETE_DEBUG_DUMP)
 *     1135a3f35bccb543
 * \23
 *
 *
 * A reply to the GET_CRASH_INFOS, CREATE_REPORT and a request REPORT
 * has the following form:
 *
 * message -> MESSAGE_TYPE data END_MARKER
 * MESSAGE_TYPE -> MESSAGE_GET_CRASH_INFOS | MESSAGE_REPORT | MESSAGE_CREATE_REPORT
 * data -> (count of items) item
 * item -> (length of member)member(length of member)memger...
 *
 * Example:
 *
 * (REPORT)
 *     (2)
 *         (4)aaaa(1)t(1)y(5)hello
 *         (3)xxx(1)s(1)n(5)world
 * \23
 *
 * The replies has same header as the requests.
 */

#define MESSAGE_DELETE_DEBUG_DUMP   "(DELETE_DEBUG_DUMP)"
#define MESSAGE_GET_CRASH_INFOS     "(GET_CRASH_INFOS)"
#define MESSAGE_REPORT              "(REPORT)"
#define MESSAGE_CREATE_REPORT       "(CREATE_REPORT)"
#define MESSAGE_END_MARKER          23

std::string crash_data_to_string(const map_crash_data_t& pCrashData)
{
    std::stringstream sCD;
    map_crash_data_t::const_iterator it_cd;
    sCD << "(" << pCrashData.size() << ")";
    for (it_cd = pCrashData.begin(); it_cd != pCrashData.end(); it_cd++)
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

std::string crash_infos_to_string(const vector_map_crash_data_t& pCrashDatas)
{
    std::stringstream sCI;
    unsigned int ii;
    for (ii = 0; ii < pCrashDatas.size(); ii++)
    {
        sCI << crash_data_to_string(pCrashDatas[ii]);
    }
    return sCI.str();
}

static int get_number_from_string(const std::string& pMessage, int& len)
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
    return xatoi(sNumber.c_str());
}

//TODO: remove constant 4 and place it in a message
map_crash_data_t string_to_crash_data(const std::string& pMessage, int& len)
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
        switch (nField % 4)
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

vector_map_crash_data_t string_to_crash_infos(const std::string& pMessage)
{
    vector_map_crash_data_t vci;
    std::string message = pMessage;
    int len;

    while (message != "")
    {
        map_crash_data_t crash_info = string_to_crash_data(message, len);
        if (crash_info.size() == 0)
        {
            return vci;
        }
        vci.push_back(crash_info);
        message.erase(0, len);
    }
    return vci;
}
