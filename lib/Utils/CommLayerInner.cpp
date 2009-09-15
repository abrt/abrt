#include <pthread.h>
#include <map>
#include "abrtlib.h"
#include "CommLayerInner.h"

static CObserver *s_pObs;

typedef std::map<uint64_t, std::string> map_uint_str_t;
static map_uint_str_t s_mapClientID;
static pthread_mutex_t s_map_mutex;
static bool s_map_mutex_inited;

void init_daemon_logging(CObserver *pObs)
{
    s_pObs = pObs;
    if (!s_map_mutex_inited)
    {
        pthread_mutex_init(&s_map_mutex, NULL);
        s_map_mutex_inited = true;
    }
}

void set_client_name(const char* name)
{
    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    if (!name)
        s_mapClientID.erase(key);
    else
        s_mapClientID[key] = name;
    pthread_mutex_unlock(&s_map_mutex);
}

void warn_client(const std::string& pMessage)
{
    if (!s_pObs)
        return;

    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (peer)
        s_pObs->Warning(pMessage, peer, key);
    else /* Bug: someone tries to warn_client() without set_client_name()!? */
        log("Hmm, stray %s: '%s'", __func__, pMessage.c_str());
}

void update_client(const std::string& pMessage)
{
    if (!s_pObs)
        return;

    uint64_t key = uint64_t(pthread_self());

    pthread_mutex_lock(&s_map_mutex);
    map_uint_str_t::const_iterator ki = s_mapClientID.find(key);
    const char* peer = (ki != s_mapClientID.end() ? ki->second.c_str() : NULL);
    pthread_mutex_unlock(&s_map_mutex);

    if (peer)
        s_pObs->Status(pMessage, peer, key);
    else
        log("Hmm, stray %s: '%s'", __func__, pMessage.c_str());
}
