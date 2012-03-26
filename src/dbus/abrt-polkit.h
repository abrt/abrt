/*
  Copyright (C) 2012  ABRT team

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
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
#ifndef ABRT_POLKIT_H
#define ABRT_POLKIT_H

#include <sys/types.h>
#include <unistd.h>

typedef enum {
/* Authorization status is unknown */
    PolkitUnknown = 0x0,
    /* Subject is authorized for the action */
    PolkitYes = 0x01,
    /* Subject is not authorized for the action */
    PolkitNo = 0x02,
    /* Challenge is needed for this action, only when flag is
     * POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE */
    PolkitChallenge = 0x03
} PolkitResult;

PolkitResult polkit_check_authorization_dname(const char *dbus_name, const char *action_id);
PolkitResult polkit_check_authorization_pid(pid_t pid, const char *action_id);

#endif
