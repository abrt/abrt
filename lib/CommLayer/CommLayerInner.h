#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include <iostream>
#include "Observer.h"

class CDebugCommLayer
{
    private:
        CObserver *m_pObserver;
    public:
        CDebugCommLayer(CObserver *pObs) :
            m_pObserver(pObs)
        {}
        void Message(const std::string& pMsg)
            {
                if(m_pObserver)
                {
                    m_pObserver->Debug(pMsg);
                }
            }
};

class CWarningCommLayer
{
    private:
        CObserver *m_pObserver;
    public:
        CWarningCommLayer(CObserver *pObs) :
            m_pObserver(pObs)
        {}
        void Message(const std::string& pMsg)
            {
                if(m_pObserver)
                {
                    m_pObserver->Warning(pMsg);
                }
            }
};

class CStatusCommLayer
{
    private:
        CObserver *m_pObserver;
    public:
        CStatusCommLayer(CObserver *pObs) :
            m_pObserver(pObs)
        {}
        void Message(const std::string& pMsg)
            {
                if(m_pObserver)
                {
                    m_pObserver->Status(pMsg);
                }
            }
};

class CCommLayerInner
{
    private:
        CDebugCommLayer* m_pDebugCommLayer;
        CWarningCommLayer* m_pWarningCommLayer;
        CStatusCommLayer* m_pStatusCommLayer;
    public:
        CDebugCommLayer* GetDebugCommLayer()
            {
                return m_pDebugCommLayer;
            }
        CWarningCommLayer* GetWarningCommLayer()
            {
                return m_pWarningCommLayer;
            }
        CStatusCommLayer* GetStatusCommLayer()
            {
                return m_pStatusCommLayer;
            }
        CCommLayerInner(CObserver *pObs, const bool& pDebug, const bool pWarning)
            {
                m_pDebugCommLayer = NULL;
                m_pWarningCommLayer = NULL;
                if (pDebug)
                {
                    m_pDebugCommLayer = new CDebugCommLayer(pObs);
                }
                if (pWarning)
                {
                    m_pWarningCommLayer = new CWarningCommLayer(pObs);
                }
                m_pStatusCommLayer = new CStatusCommLayer(pObs);
            }
        ~CCommLayerInner()
            {
                if (m_pDebugCommLayer)
                {
                    delete m_pDebugCommLayer;
                }
                if (m_pWarningCommLayer)
                {
                    delete m_pWarningCommLayer;
                }
                delete m_pStatusCommLayer;
            }
};

#endif /* COMMLAYERINNER_H_ */
