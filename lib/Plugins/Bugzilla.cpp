#include "Bugzilla.h"
#include <xmlrpc-c/base.hpp>
#include "CrashTypes.h"
#include "PluginSettings.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include <sstream>
#include <string.h>

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

PRInt32 CReporterBugzilla::Base64Encode_cb(void *arg, const char *obuf, PRInt32 size)
{
    CReporterBugzilla* bz = static_cast<CReporterBugzilla*>(arg);
    bz->m_sAttchmentInBase64 += obuf;
    return 1;
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
        std::stringstream ss;
        ss << xmlrpc_c::value_int(ret["id"]);
        comm_layer_inner_debug("Login id: " + ss.str());
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
                   "\n\nHow to reproduce\n"
                   "-----\n" +
                   pCrashReport.find(CD_REPRODUCE)->second[CD_CONTENT] +
                   "\n\nCommnet\n"
                   "-----\n" +
                   pCrashReport.find(CD_COMMENT)->second[CD_CONTENT] +
                   "\n\nAdditional information\n"
                   "======\n";

    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first !=  CD_UUID &&
                it->first !=  FILENAME_ARCHITECTURE &&
                it->first !=  FILENAME_RELEASE &&
                it->first !=  CD_REPRODUCE &&
                it->first !=  CD_COMMENT)
            {
                pDescription += "\n" + it->first + "\n";
                pDescription += "-----\n";
                pDescription += it->second[CD_CONTENT] + "\n\n";
            }
        }
        else if (it->second[CD_TYPE] == CD_ATT)
        {
            pDescription += "\n\nAttached files\n"
                            "----\n";
            pDescription += it->first + "\n";
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            comm_layer_inner_warning("Binary file "+it->first+" will not be reported.");
            comm_layer_inner_status("Binary file "+it->first+" will not be reported.");
        }
    }
}

void CReporterBugzilla::GetProductAndVersion(const std::string& pRelease,
                                             std::string& pProduct,
                                             std::string& pVersion)
{
    if (pRelease.find("Rawhide") != std::string::npos)
    {
        pProduct = "Fedora";
        pVersion = "rawhide";
        return;
    }
    if (pRelease.find("Fedora") != std::string::npos)
    {
        pProduct = "Fedora";
    }
    else if (pRelease.find("Red Hat Enterprise Linux") != std::string::npos)
    {
        pProduct = "Red Hat Enterprise Linux ";
    }
    std::string::size_type pos = pRelease.find("release");
    pos = pRelease.find(" ", pos) + 1;
    while (pRelease[pos] != ' ')
    {
        pVersion += pRelease[pos];
        if (pProduct == "Red Hat Enterprise Linux ")
        {
            pProduct += pRelease[pos];
        }
        pos++;
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
    //bugParams["op_sys"] =  xmlrpc_c::value_string("Linux");
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
        std::stringstream ss;
        ss << xmlrpc_c::value_int(ret["id"]);
        comm_layer_inner_debug("New bug id: " + ss.str());
        AddAttachments(ss.str(), pCrashReport);
    }
    catch (std::exception& e)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::NewBug(): ") + e.what());
    }

}

void CReporterBugzilla::AddAttachments(const std::string& pBugId, const map_crash_report_t& pCrashReport)
{
    xmlrpc_c::paramList paramList;
    map_xmlrpc_params_t attachmentParams;
    std::vector<xmlrpc_c::value> ret;
    NSSBase64Encoder* base64;
    std::string::size_type pos;

    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_ATT)
        {
            base64 = NSSBase64Encoder_Create(Base64Encode_cb, this);
            NSSBase64Encoder_Update(base64,
                                    reinterpret_cast<const unsigned char*>(it->second[CD_CONTENT].c_str()),
                                    it->second[CD_CONTENT].length());
            NSSBase64Encoder_Destroy(base64, PR_FALSE);
            std::string attchmentInBase64Printable = "";
            for(unsigned int ii = 0; ii < m_sAttchmentInBase64.length(); ii++)
            {
                if (isprint(m_sAttchmentInBase64[ii]))
                {
                    attchmentInBase64Printable +=  m_sAttchmentInBase64[ii];
                }
            }
            paramList.add(xmlrpc_c::value_string(pBugId));
            attachmentParams["description"] = xmlrpc_c::value_string("File: " + it->first);
            attachmentParams["filename"] = xmlrpc_c::value_string(it->first);
            attachmentParams["contenttype"] = xmlrpc_c::value_string("text/plain");
            attachmentParams["data"] = xmlrpc_c::value_string(attchmentInBase64Printable);
            paramList.add(xmlrpc_c::value_struct(attachmentParams));
            xmlrpc_c::rpcPtr rpc(new  xmlrpc_c::rpc("bugzilla.addAttachment", paramList));
            try
            {
                rpc->call(m_pXmlrpcClient, m_pCarriageParm);
                ret =  xmlrpc_c::value_array(rpc->getResult()).vectorValueValue();
                std::stringstream ss;
                ss << xmlrpc_c::value_int(ret[0]);
                comm_layer_inner_debug("New attachment id: " + ss.str());
            }
            catch (std::exception& e)
            {
                throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::AddAttachemnt(): ") + e.what());
            }
        }
    }
}

void CReporterBugzilla::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    std::string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    std::string component = package.substr(0, package.rfind("-", package.rfind("-")-1));
    std::string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];
    comm_layer_inner_status("Logging into bugzilla...");
    Login();
    comm_layer_inner_status("Checking for duplicates...");
    if (!CheckUUIDInBugzilla(component, uuid))
    {
        comm_layer_inner_status("Creating new bug...");
        NewBug(pCrashReport);
    }
    comm_layer_inner_status("Logging out...");
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
