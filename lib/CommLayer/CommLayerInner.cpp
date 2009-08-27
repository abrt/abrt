#include <pthread.h> /* pthread_self() */
#include "abrtlib.h"
#include "CommLayerInner.h"

static CObserver *s_pObs;
static pthread_t s_main_id;

void init_daemon_logging(CObserver *pObs)
{
    s_pObs = pObs;
    s_main_id = pthread_self();
}

void warn_client(const std::string& pMessage)
{
    if (!s_pObs)
        return;
    pthread_t self = pthread_self();
    if (self != s_main_id)
    {
        s_pObs->Warning(pMessage,(uint64_t)self);
//log("w: '%s'", s.c_str());
    }
    else
    {
        s_pObs->Warning(pMessage);
// debug: this should not happen - if it is, we are trying to log to a client
// but we have no job id!
log("W: '%s'", pMessage.c_str());
    }
}

void update_client(const std::string& pMessage)
{
    if (!s_pObs)
        return;
    pthread_t self = pthread_self();
    if (self != s_main_id)
    {
        s_pObs->Status(pMessage, (uint64_t)self);
//log("u: '%s'", s.c_str());
    }
    else
    {
        s_pObs->Status(pMessage);
// debug: this should not happen - if it is, we are trying to log to a client
// but we have no job id!
log("U: '%s'", pMessage.c_str());
    }
}
