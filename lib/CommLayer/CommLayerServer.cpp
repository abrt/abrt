#include "CommLayerServer.h"
#include <iostream>

CCommLayerServer::CCommLayerServer()
{
}

CCommLayerServer::~CCommLayerServer()
{
}

void CCommLayerServer::Attach(CObserver *pObs)
{
    m_pObserver = pObs;
}
void CCommLayerServer::Detach(CObserver *pObs)
{
    m_pObserver = NULL;
}
void CCommLayerServer::Notify(const std::string& pMessage)
{
    if(m_pObserver)
        m_pObserver->StatusUpdate(pMessage);
}
