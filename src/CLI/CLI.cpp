#include "ABRTException.h"
#include "ABRTSocket.h"
#include "ABRTException.h"
#include <iostream>
#include <CommLayerClientDBus.h>
#include <string.h>

#define SOCKET_FILE VAR_RUN"/abrt.socket"

typedef enum {HELP,
              GET_LIST,
              GET_LIST_FULL,
              REPORT,
              REPORT_ALWAYS,
              DELETE} param_mode_t;

typedef struct param_s
{
    param_mode_t m_Mode;
    char* m_sUUID;
} param_t;

void print_usage(char* pProgramName)
{
    std::cout << pProgramName << " [OPTION]" << std::endl << std::endl;
    std::cout << "[OPTION]" << std::endl;
    std::cout << "\t--help                   - prints this text" << std::endl;
    std::cout << "\t--get-list               - prints list of crashes which are not reported" << std::endl;
    std::cout << "\t--get-list-full          - prints list of all crashes" << std::endl;
    std::cout << "\t--report <uuid>          - create and send a report" << std::endl;
    std::cout << "\t--report-always <uuid>   - create and send a report without asking" << std::endl;
    std::cout << "\t--delete <uuid>          - delete crash" << std::endl;
}

void parse_args(int argc, char** argv, param_t& param)
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "--version"))
        {
            param.m_Mode = HELP;
        }
        else if (!strcmp(argv[1], "--get-list"))
        {
            param.m_Mode = GET_LIST;
        }
        else if (!strcmp(argv[1], "--get-list-full"))
        {
            param.m_Mode = GET_LIST_FULL;
        }
        else
        {
            param.m_Mode = HELP;
        }
    }
    else if (argc == 3)
    {
        if (!strcmp(argv[1], "--report"))
        {
            param.m_Mode = REPORT;
            param.m_sUUID = argv[2];
        }
        else if (!strcmp(argv[1], "--report-always"))
        {
            param.m_Mode = REPORT_ALWAYS;
            param.m_sUUID = argv[2];
        }
        else if (!strcmp(argv[1], "--delete"))
        {
            param.m_Mode = DELETE;
            param.m_sUUID = argv[2];
        }
        else
        {
            param.m_Mode = HELP;
        }
    }
    else
    {
        param.m_Mode = HELP;
    }
}

void print_crash_infos(const vector_crash_infos_t& pCrashInfos,
                       const param_mode_t& pMode)
{
    unsigned int ii;
    for (ii = 0; ii < pCrashInfos.size(); ii++)
    {
        if (pCrashInfos[ii].find(CD_REPORTED)->second[CD_CONTENT] != "1" || pMode == GET_LIST_FULL)
        {
            std::cout << ii << ". " << std::endl;
            std::cout << "\tUID       : " << pCrashInfos[ii].find(CD_UID)->second[CD_CONTENT] << std::endl;
            std::cout << "\tUUID      : " << pCrashInfos[ii].find(CD_UUID)->second[CD_CONTENT] << std::endl;
            std::cout << "\tPackage   : " << pCrashInfos[ii].find(CD_PACKAGE)->second[CD_CONTENT] << std::endl;
            std::cout << "\tExecutable: " << pCrashInfos[ii].find(CD_EXECUTABLE)->second[CD_CONTENT] << std::endl;
            std::cout << "\tCrash time: " << pCrashInfos[ii].find(CD_TIME)->second[CD_CONTENT] << std::endl;
            std::cout << "\tCrash Rate: " << pCrashInfos[ii].find(CD_COUNT)->second[CD_CONTENT] << std::endl;
        }
    }
}

void print_crash_report(const map_crash_report_t& pCrashReport)
{
    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] != CD_SYS)
        {
            std::cout << std::endl << it->first << std::endl;
            std::cout << "-----" << std::endl;
            std::cout << it->second[CD_CONTENT] << std::endl;
        }
    }
}

int main(int argc, char** argv)
{
    vector_crash_infos_t ci;
    map_crash_report_t cr;
    param_t param;
    std::string answer = "n";

    parse_args(argc, argv, param);

    if (param.m_Mode == HELP)
    {
        print_usage(argv[0]);
        return 1;
    }
    try
    {
#ifdef ENABLE_DBUS
        DBus::Glib::BusDispatcher dispatcher;
        /* this should bind the dispatcher with mainloop */
        dispatcher.attach(NULL);
        DBus::default_dispatcher = &dispatcher;
        DBus::Connection conn = DBus::Connection::SystemBus();
        CCommLayerClientDBus ABRTDaemon(conn, CC_DBUS_PATH, CC_DBUS_NAME);
        if(!conn.has_name(CC_DBUS_NAME)){
            std::cout << "Daemon is not running!" << std::endl;
            return -1;
        }
#elif ENABLE_SOCKET
        CABRTSocket ABRTDaemon;
        ABRTDaemon.Connect(SOCKET_FILE);
#endif
        switch (param.m_Mode)
        {
            case GET_LIST:
                ci = ABRTDaemon.GetCrashInfos();
                print_crash_infos(ci, GET_LIST);
                break;
            case GET_LIST_FULL:
                ci = ABRTDaemon.GetCrashInfos();
                print_crash_infos(ci, GET_LIST_FULL);
                break;
            case REPORT:
                cr = ABRTDaemon.CreateReport(param.m_sUUID);
                print_crash_report(cr);
                std::cout << std::endl << "Do you want to send the report? [y/n]: ";
                std::flush(std::cout);
                std::cin >> answer;
                if (answer == "Y" || answer == "y")
                {
                    ABRTDaemon.Report(cr);
                }
                break;
            case REPORT_ALWAYS:
                cr = ABRTDaemon.CreateReport(param.m_sUUID);
                ABRTDaemon.Report(cr);
                break;
            case DELETE:
                ABRTDaemon.DeleteDebugDump(param.m_sUUID);
                break;
            default:
                print_usage(argv[0]);
                break;
        }
#if ENABLE_SOCKET
        ABRTDaemon.DisConnect();
#endif

    }
    catch (CABRTException& e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
