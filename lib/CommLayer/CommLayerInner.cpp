#include "CommLayerInner.h"

namespace CommLayerInner
{

    static CDebug* g_pDebug = NULL;
    static CWarning* g_pWarning = NULL;
    static CStatus* g_pStatus = NULL;


    void init_debug(CObserver* pObserver)
    {
        if (!g_pDebug)
        {
            g_pDebug = new CDebug(pObserver);
        }
    }

    void init_warning(CObserver* pObserver)
    {
        if (!g_pWarning)
        {
            g_pWarning = new CWarning(pObserver);
        }
    }

    void init_status(CObserver* pObserver)
    {
        if (!g_pStatus)
        {
            g_pStatus = new CStatus(pObserver);
        }
    }

    void debug(const std::string& pMessage)
    {
        if (g_pDebug)
        {
            g_pDebug->Message(pMessage);
        }
    }
    void warning(const std::string& pMessage)
    {
        if (g_pWarning)
        {
            g_pWarning->Message(pMessage);
        }
    }

    void status(const std::string& pMessage)
    {
        if (g_pStatus)
        {
            g_pStatus->Message(pMessage);
        }
    }
}
