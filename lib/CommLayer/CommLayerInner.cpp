#include "CommLayerInner.h"

static CObserver *g_pObs = NULL;


void comm_layer_inner_init(CObserver *pObs)
{
    if (!g_pObs)
    {
        g_pObs = pObs;
    }
}

void comm_layer_inner_debug(const std::string& pMessage)
{
    if (g_pObs)
    {
        g_pObs->Debug(pMessage);
    }
}
void comm_layer_inner_warning(const std::string& pMessage)
{
    if (g_pObs)
    {
        g_pObs->Warning(pMessage);
    }
}

void comm_layer_inner_status(const std::string& pMessage)
{
    if (g_pObs)
    {
        g_pObs->Status(pMessage);
    }
}
