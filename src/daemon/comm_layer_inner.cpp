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
#include <pthread.h>
#include <map>
#include "abrtlib.h"
#include "CommLayerServerDBus.h"
#include "comm_layer_inner.h"

typedef std::map<uint64_t, std::string> map_uint_str_t;
static map_uint_str_t s_mapClientID;
static pthread_mutex_t s_map_mutex;
static bool s_map_mutex_inited;

/* called via [p]error_msg() */
static void warn_client(const char *msg)
{
    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (peer)
    {
        send_dbus_sig_Warning(msg, peer);
    }
}

void init_daemon_logging(void)
{
    if (!s_map_mutex_inited)
    {
        s_map_mutex_inited = true;
        pthread_mutex_init(&s_map_mutex, NULL);
        g_custom_logger = &warn_client;
    }
}

void set_client_name(const char *name)
{
    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    if (!name) {
        s_mapClientID.erase(key);
    } else {
        s_mapClientID[key] = name;
    }
    pthread_mutex_unlock(&s_map_mutex);
}

void update_client(const char *fmt, ...)
{
    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (!peer)
        return;

    va_list p;
    va_start(p, fmt);
    char *msg = xvasprintf(fmt, p);
    va_end(p);

    VERB1 log("Update('%s'): %s", peer, msg);
    send_dbus_sig_Update(msg, peer);

    free(msg);
}
