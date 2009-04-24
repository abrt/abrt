#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include <iostream>
#include "Observer.h"

namespace CommLayerInner
{

    class CDebug
    {
        private:
            CObserver *m_pObs;
        public:
            CDebug(CObserver *pObs) :
                m_pObs(pObs)
            {}
            void Message(const std::string& pMsg)
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
            CWarning(CObserver *pObs) :
                m_pObs(pObs)
            {}
            void Message(const std::string& pMsg)
                {
                    if(m_pObs)
                        m_pObs->Warning(pMsg);
                }
    };

    class CStatus
    {
        private:
            CObserver *m_pObs;
        public:
            CStatus(CObserver *pObs) :
                m_pObs(pObs)
            {}
            void Message(const std::string& pMsg)
                {
                    if(m_pObs)
                        m_pObs->Status(pMsg);
                }
    };


    void init_debug(CObserver* pObserver);
    void init_warning(CObserver* pObserver);
    void init_status(CObserver* pObserver);

    void debug(const std::string& pMessage);
    void warning(const std::string& pMessage);
    void status(const std::string& pMessage);
};

#endif /* COMMLAYERINNER_H_ */
