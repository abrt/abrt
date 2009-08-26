#include "CommLayerServer.h"
#include "CrashWatcher.h"

CCommLayerServer::CCommLayerServer()
{
}

CCommLayerServer::~CCommLayerServer()
{
}

void CCommLayerServer::Attach(CCrashWatcher *pCW)
{
    m_pCrashWatcher = pCW;
}
void CCommLayerServer::Detach(CCrashWatcher *pCW)
{
    m_pCrashWatcher = NULL;
}
void CCommLayerServer::Notify(const std::string& pMessage)
{
    if (m_pCrashWatcher)
        m_pCrashWatcher->Status(pMessage);
}
