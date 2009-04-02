#include <vector>
#include <map>
#include <string>
#include <sstream>
#include "MiddleWare.h"
#include "Observer.h"

/* just a helper function */
template< class T >
std::string
to_string( T x )
{
    std::ostringstream o;
    o << x;
    return o.str();
}

class CCommLayerServer{
    private:
        /* FIXME more observers? */
        //std::vector<Observer *obs>;
        CObserver *m_pObserver;
    public:
        CMiddleWare *m_pMW;
        CCommLayerServer(CMiddleWare *pMW);
        virtual ~CCommLayerServer();
        /* observer */
        void Attach(CObserver *pObs);
        void Detach(CObserver *pObs);
        void Notify(const std::string& pMessage);
        /*
     virtual dbus_vector_crash_infos_t GetCrashInfos(const std::string &pUID) = 0;
     virtual dbus_vector_map_crash_infos_t GetCrashInfosMap(const std::string &pDBusSender) = 0;
     virtual dbus_map_report_info_t CreateReport(const std::string &pUUID,const std::string &pDBusSender) = 0;
     virtual bool Report(dbus_map_report_info_t pReport) = 0;
     virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender) = 0;
     */
    public:
    /* just stubs to be called when not implemented in specific comm layer */
        void Crash(const std::string& arg1) {}
        void AnalyzeComplete(map_crash_report_t arg1) {}
        void Error(const std::string& arg1) {}
};
