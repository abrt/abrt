#include "CommLayerServerSocket.h"
#include <iostream>


CCommLayerServerSocket::CCommLayerServerSocket(CMiddleWare *pMW)
: CCommLayerServer(pMW)
{
    std::cout << "CCommLayerServerSocket init" << std::endl;
}

CCommLayerServerSocket::~CCommLayerServerSocket()
{
    std::cout << "Cleaning up Socket" << std::endl;
}
