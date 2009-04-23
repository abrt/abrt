#include "CommLayerServerSocket.h"
#include <iostream>


CCommLayerServerSocket::CCommLayerServerSocket()
: CCommLayerServer()
{
    std::cout << "CCommLayerServerSocket init" << std::endl;
}

CCommLayerServerSocket::~CCommLayerServerSocket()
{
    std::cout << "Cleaning up Socket" << std::endl;
}
