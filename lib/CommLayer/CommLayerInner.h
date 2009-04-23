#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include <iostream>
#include "Observer.h"

class CDebug
{
    private:
        CObserver *m_pObs;
    public:
        CDebug(CObserver *pObs){ m_pObs = pObs; }
        
        void operator << (const std::string& pMsg)
            {
                if(m_pObs)
                    m_pObs->Debug(pMsg);
            }
};

class CWarning
{
    private:
        CObserver *m_pObs;
    public:
        CWarning(CObserver *pObs){ m_pObs = pObs; }
        
        void operator << (const std::string& pMsg)
            {
                if(m_pObs)
                    m_pObs->Warning(pMsg);
            }
};

class CStatusUpdate
{
    private:
        CObserver *m_pObs;
    public:
        CStatusUpdate(CObserver *pObs){ m_pObs = pObs; }
        
        void operator << (const std::string& pMsg)
            {
                if(m_pObs)
                    m_pObs->StatusUpdate(pMsg);
            }
};

class CCommLayerInner{
    private:
        CObserver *m_pObs;
    public:
        CDebug DEBUGINFO;
        CWarning WARNING;
        CStatusUpdate STATUS;
        CCommLayerInner(CObserver *pObs);
        ~CCommLayerInner();
        CDebug& Debug();
        CWarning& Warning();
        CStatusUpdate& Status();
};

#endif /* COMMLAYERINNER_H_ */
