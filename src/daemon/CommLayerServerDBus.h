/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#ifndef COMMLAYERSERVERDBUS_H_
#define COMMLAYERSERVERDBUS_H_

#ifdef __cplusplus
extern "C" {
#endif

int init_dbus(void);
void deinit_dbus(void);

void send_dbus_sig_Crash(const char *package_name,
                        const char *dir,
                        const char *uid_str
);
void send_dbus_sig_QuotaExceeded(const char* str);

void send_dbus_sig_Update(const char* pMessage, const char* peer);
void send_dbus_sig_Warning(const char* pMessage, const char* peer);

#ifdef __cplusplus
}
#endif

#endif
