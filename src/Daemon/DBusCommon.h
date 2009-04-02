/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
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

#define CC_DBUS_NAME "com.redhat.abrt"
#define CC_DBUS_PATH "/com/redhat/abrt"
#define CC_DBUS_IFACE "com.redhat.abrt"

#include <map>
#include <string>
#include <vector>

//typedef std::vector<crash_info_t> vector_crash_infos_t;
typedef std::vector< std::vector<std::string> > vector_crash_infos_t;
typedef std::vector< std::map<std::string, std::string> > dbus_vector_map_crash_infos_t;
//typedef std::map<std::string, std::string> dbus_map_report_info_t;
typedef std::vector<std::string> map_crash_report_t;
