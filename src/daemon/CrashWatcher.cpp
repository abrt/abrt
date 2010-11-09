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
#include "abrtlib.h"
#include "Daemon.h"
#include "CommLayerServer.h"
#include "CrashWatcher.h"

void CCrashWatcher::Status(const char *pMessage, const char* peer)
{
    VERB1 log("Update('%s'): %s", peer, pMessage);
    if (g_pCommLayer != NULL)
        g_pCommLayer->Update(pMessage, peer);
}

void CCrashWatcher::Warning(const char *pMessage, const char* peer)
{
    VERB1 log("Warning('%s'): %s", peer, pMessage);
    if (g_pCommLayer != NULL)
        g_pCommLayer->Warning(pMessage, peer);
}

CCrashWatcher::CCrashWatcher()
{
}

CCrashWatcher::~CCrashWatcher()
{
}
