/*
    CrashTypesSocket.h - contains inline functions for socket communication

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

#ifndef SOCKETCRASHTYPES_H_
#define SOCKETCRASHTYPES_H_

#include "CrashTypes.h"

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

std::string crash_infos_to_string(const vector_crash_infos_t& pCrashInfos);
std::string crash_data_to_string(const map_crash_data_t& pCrashData);
inline std::string crash_report_to_string(const map_crash_report_t& pCrashReport)
{
    return crash_data_to_string(pCrashReport);
}

vector_crash_infos_t string_to_crash_infos(const std::string& pMessage);
map_crash_data_t string_to_crash_data(const std::string& pMessage, int& len);
inline map_crash_report_t string_to_crash_report(const std::string& pMessage)
{
    int len;
    return string_to_crash_data(pMessage, len);
}

#endif /* SOCKETCRASHTYPES_H_ */
