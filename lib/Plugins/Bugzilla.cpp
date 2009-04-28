#include "Bugzilla.h"
#include <xmlrpc-c/base.hpp>
#include "CrashTypes.h"
#include "PluginSettings.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "ABRTCommLayer.h"
#include <iostream>

CReporterBugzilla::CReporterBugzilla() :
    m_sBugzillaURL("https://bugzilla.redhat.com/xmlrpc.cgi")
{
    m_pXmlrpcTransport = new xmlrpc_c::clientXmlTransport_curl();
    m_pXmlrpcClient = new xmlrpc_c::client_xml(m_pXmlrpcTransport);
    m_pCarriageParm = new xmlrpc_c::carriageParm_curl0(m_sBugzillaURL);
}

CReporterBugzilla::~CReporterBugzilla()
{
    delete m_pXmlrpcTransport;
    delete m_pXmlrpcClient;
    delete m_pCarriageParm;
}

void CReporterBugzilla::Login()
{
    xmlrpc_c::paramList paramList;
    map_xmlrpc_params_t loginParams;
    map_xmlrpc_params_t ret;
    loginParams["login"] = xmlrpc_c::value_string(m_sLogin);
    loginParams["password"] = xmlrpc_c::value_string(m_sPassword);
    paramList.add(xmlrpc_c::value_struct(loginParams));
    xmlrpc_c::rpcPtr rpc(new  xmlrpc_c::rpc("User.login", paramList));
    try
    {
        rpc->call(m_pXmlrpcClient, m_pCarriageParm);
        ret =  xmlrpc_c::value_struct(rpc->getResult());
        ABRTCommLayer::debug("Login id: " + xmlrpc_c::value_int(ret["id"]));
    }
    catch (std::exception& e)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::Login(): ") + e.what());
    }
}

void CReporterBugzilla::Logout()
{
    xmlrpc_c::paramList paramList;
    paramList.add(xmlrpc_c::value_string(""));
    xmlrpc_c::rpcPtr rpc(new  xmlrpc_c::rpc("User.logout", paramList));
    try
    {
        rpc->call(m_pXmlrpcClient, m_pCarriageParm);
    }
    catch (std::exception& e)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::Logout(): ") + e.what());
    }
}

bool CReporterBugzilla::CheckUUIDInBugzilla(const std::string& pComponent, const std::string& pUUID)
{
    xmlrpc_c::paramList paramList;
    map_xmlrpc_params_t searchParams;
    map_xmlrpc_params_t ret;
    std::string quicksearch = "ALL component:\""+ pComponent +"\" statuswhiteboard:\""+ pUUID + "\"";
    searchParams["quicksearch"] = xmlrpc_c::value_string(quicksearch.c_str());
    paramList.add(xmlrpc_c::value_struct(searchParams));
    xmlrpc_c::rpcPtr rpc(new  xmlrpc_c::rpc("Bug.search", paramList));
    try
    {
        rpc->call(m_pXmlrpcClient, m_pCarriageParm);
    }
    catch (std::exception& e)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::CheckUUIDInBugzilla(): ") + e.what());
    }
    ret = xmlrpc_c::value_struct(rpc->getResult());
    std::vector<xmlrpc_c::value> bugs = xmlrpc_c::value_array(ret["bugs"]).vectorValueValue();
    if (bugs.size() > 0)
    {
        return true;
    }
    return false;
}

void CReporterBugzilla::CreateNewBugDescription(const map_crash_report_t& pCrashReport, std::string& pDescription)
{
    pDescription = "\nabrt detected crash.\n"
                   "\nHow to reproduce\n" +
                   pCrashReport.find(CD_REPRODUCE)->second[CD_CONTENT] +
                   "\nCommnet\n" +
                   pCrashReport.find(CD_COMMENT)->second[CD_CONTENT] +
                   "\nAdditional information\n"
                   "======\n";

    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT || it->second[CD_TYPE] == CD_ATT)
        {
            if (it->first !=  CD_UUID &&
                it->first !=  FILENAME_ARCHITECTURE &&
                it->first !=  FILENAME_RELEASE &&
                it->first !=  CD_REPRODUCE &&
                it->first !=  CD_COMMENT)
            {
                pDescription += it->first + "\n";
                pDescription += "-----\n";
                pDescription += it->second[CD_CONTENT] + "\n\n";
            }
        }
    }
}

void CReporterBugzilla::GetProductAndVersion(const std::string& pRelease,
                                             std::string& pProduct,
                                             std::string& pVersion)
{
    // pattern: Fedora release version (codename)
    // TODO: Consider other distribution
    pProduct = pRelease.substr(0, pRelease.find(" "));
    pVersion = pRelease.substr(pRelease.find(" ", pRelease.find(" ") + 1) + 1, 2);
    if (pVersion == "")
    {
        pVersion = "rawhide";
    }
}

void CReporterBugzilla::NewBug(const map_crash_report_t& pCrashReport)
{
    xmlrpc_c::paramList paramList;
    map_xmlrpc_params_t bugParams;
    map_xmlrpc_params_t ret;
    std::string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    std::string component = package.substr(0, package.rfind("-", package.rfind("-")-1));
    std::string description;
    std::string release = pCrashReport.find(FILENAME_RELEASE)->second[CD_CONTENT];;
    std::string product;
    std::string version;
    CreateNewBugDescription(pCrashReport, description);
    GetProductAndVersion(release, product, version);


    bugParams["product"] = xmlrpc_c::value_string(product);
    bugParams["component"] =  xmlrpc_c::value_string(component);
    bugParams["version"] =  xmlrpc_c::value_string(version);
    bugParams["summary"] = xmlrpc_c::value_string("[abrt] crash detected in " + component);
    bugParams["description"] = xmlrpc_c::value_string(description);
    bugParams["status_whiteboard"] = xmlrpc_c::value_string("abrt_hash:" + pCrashReport.find(CD_UUID)->second[CD_CONTENT]);
    bugParams["platform"] = xmlrpc_c::value_string(pCrashReport.find(FILENAME_ARCHITECTURE)->second[CD_CONTENT]);
    paramList.add(xmlrpc_c::value_struct(bugParams));

    xmlrpc_c::rpcPtr rpc(new  xmlrpc_c::rpc("Bug.create", paramList));
    try
    {
        rpc->call(m_pXmlrpcClient, m_pCarriageParm);
        ret =  xmlrpc_c::value_struct(rpc->getResult());
        ABRTCommLayer::debug("New bug id: " + xmlrpc_c::value_int(ret["id"]));
    }
    catch (std::exception& e)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::NewBug(): ") + e.what());
    }

}

void CReporterBugzilla::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    std::string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    std::string component = package.substr(0, package.rfind("-", package.rfind("-")-1));
    std::string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];
    ABRTCommLayer::status("Logging into bugzilla...");
    Login();
    ABRTCommLayer::status("Checking for duplicates...");
    if (!CheckUUIDInBugzilla(component, uuid))
    {
        ABRTCommLayer::status("Creating new bug...");
        NewBug(pCrashReport);
    }
    ABRTCommLayer::status("Logging out...");
    Logout();
}

void CReporterBugzilla::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    plugin_load_settings(pPath, settings);

    if (settings.find("BugzillaURL")!= settings.end())
    {
        m_sBugzillaURL = settings["BugzillaURL"];
    }
    if (settings.find("Login")!= settings.end())
    {
        m_sLogin = settings["Login"];
    }
    if (settings.find("Password")!= settings.end())
    {
        m_sPassword = settings["Password"];
    }
}
