#include "CommLayerServer.h"

class CCommLayerServerSocket
: public CCommLayerServer
{
    private:
    public:
        CCommLayerServerSocket(CMiddleWare *pMW);
        ~CCommLayerServerSocket();
};
