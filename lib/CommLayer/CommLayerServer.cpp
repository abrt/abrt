#include "CommLayerServer.h"
#include <iostream>

CCommLayerServer::CCommLayerServer(CMiddleWare *pMW)
{
    m_pMW = pMW;
    std::cerr << "CCommLayerServer init.." << std::endl;
}

CCommLayerServer::~CCommLayerServer()
{
    std::cout << "CCommLayerServer::Cleaning up.." << std::endl;
}

void CCommLayerServer::Attach(CObserver *pObs)
{
    std::cerr << "CCommLayerServer::Attach" << std::endl;
    m_pObserver = pObs;
}
void CCommLayerServer::Detach(CObserver *pObs)
{
    std::cerr << "CCommLayerServer::Detach" << std::endl;
    m_pObserver = NULL;
}
void CCommLayerServer::Notify(const std::string& pMessage)
{
    std::cerr << "CCommLayerServer::Notify" << std::endl;
    if(m_pObserver)
        m_pObserver->Update(pMessage);
}
