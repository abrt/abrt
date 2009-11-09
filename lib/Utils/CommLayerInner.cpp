#include <pthread.h>
#include <map>
#include "abrtlib.h"
#include "CommLayerInner.h"

static CObserver *s_pObs;

typedef std::map<uint64_t, std::string> map_uint_str_t;
static map_uint_str_t s_mapClientID;
static pthread_mutex_t s_map_mutex;
static bool s_map_mutex_inited;

/* called via [p]error_msg() */
static void warn_client(const char *msg)
{
    if (!s_pObs)
        return;

    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (peer)
        s_pObs->Warning(msg, peer, key);
}

void init_daemon_logging(CObserver *pObs)
{
    s_pObs = pObs;
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
    if (!s_pObs)
        return;

    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (!peer)
        return;

    va_list p;
    va_start(p, fmt);
    char *msg;
    int used = vasprintf(&msg, fmt, p);
    va_end(p);
    if (used < 0)
	return;

    s_pObs->Status(msg, peer, key);
}
