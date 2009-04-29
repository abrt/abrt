#include "CommLayerInner.h"

CCommLayerInner* g_pCommLayerInner = NULL;


void comm_layer_inner_init(CCommLayerInner *pCommLayerInner)
{
    if (!g_pCommLayerInner)
    {
        g_pCommLayerInner = pCommLayerInner;
    }
}

void comm_layer_inner_debug(const std::string& pMessage)
{
    if (g_pCommLayerInner)
    {
        if (g_pCommLayerInner->GetDebugCommLayer())
        {
            g_pCommLayerInner->GetDebugCommLayer()->Message(pMessage);
        }
    }
}
void comm_layer_inner_warning(const std::string& pMessage)
{
    if (g_pCommLayerInner)
    {
        if (g_pCommLayerInner->GetWarningCommLayer())
        {
            g_pCommLayerInner->GetWarningCommLayer()->Message(pMessage);
        }
    }
}

void comm_layer_inner_status(const std::string& pMessage)
{
    if (g_pCommLayerInner)
    {
        if (g_pCommLayerInner->GetStatusCommLayer())
        {
            g_pCommLayerInner->GetStatusCommLayer()->Message(pMessage);
        }
    }
}
