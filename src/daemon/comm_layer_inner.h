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
#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#ifdef __cplusplus
extern "C" {
#endif

void init_daemon_logging(void);

/*
 * Set client's name (dbus ID). NULL unsets it.
 */
void set_client_name(const char* name);

/*
 * Ask a client to warn the user about a non-fatal, but unexpected condition.
 * In GUI, it will usually be presented as a popup message.
 * Usually there is no need to call it directly, just use [p]error_msg().
 */
//now static:
//void warn_client(const char *msg);
//use [p]error_msg[_and_die] instead, it sends the message as a warning to client
//as well as to the log.

/*
 * Logs a message to a client.
 * In UI, it will usually appear as a new status line message in GUI,
 * or as a new message line in CLI.
 */
void update_client(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
