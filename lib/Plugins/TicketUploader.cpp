/*
    TicketUploader.cpp

    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    */
#include <string>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include "abrtlib.h"
#include "TicketUploader.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

using namespace std;


CTicketUploader::CTicketUploader() :
    m_bEncrypt(false),
    m_bUpload(false),
    m_nRetryCount(3),
    m_nRetryDelay(20)
{}

CTicketUploader::~CTicketUploader()
{}


static void Error(const char *msg)
{
    update_client("%s", msg);
    throw CABRTException(EXCEP_PLUGIN, msg);
}

static void RunCommand(const char *cmd)
{
    int retcode = system(cmd);
    if (retcode)
    {
        Error(ssprintf("'%s' exited with %d", cmd, retcode).c_str());
    }
}

static string ReadCommand(const char *cmd)
{
    FILE* fp = popen(cmd, "r");
    if (!fp)
    {
        Error(ssprintf("error running '%s'", cmd).c_str());
    }

    string result;
    char buff[1024];
    while (fgets(buff, sizeof(buff), fp) != NULL)
    {
        result += buff;
    }

    int retcode = pclose(fp);
    if (retcode)
    {
        Error(ssprintf("'%s' exited with %d", cmd, retcode).c_str());
    }

    return result;
}

static void WriteCommand(const char *cmd, const char *input)
{
    FILE* fp = popen(cmd, "w");
    if (!fp)
    {
        Error(ssprintf("error running '%s'", cmd).c_str());
    }

    /* Hoping it's not too big to get us forever blocked... */
    fputs(input, fp);

    int retcode = pclose(fp);
    if (retcode)
    {
        Error(ssprintf("'%s' exited with %d", cmd, retcode).c_str());
    }
}

void CTicketUploader::SendFile(const char *pURL, const char *pFilename)
{
    if (pURL[0] == '\0')
    {
        error_msg(_("FileTransfer: URL not specified"));
        return;
    }

    update_client(_("Sending archive %s to %s"), pFilename, pURL);

    const char *base = (strrchr(pFilename, '/') ? : pFilename-1) + 1;
    string wholeURL = concat_path_file(pURL, base);
    int count = m_nRetryCount;
    int result;
    do
    {
        FILE* f = fopen(pFilename, "r");
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, ssprintf("Can't open archive file '%s'", pFilename));
        }
        struct stat buf;
        if (fstat(fileno(f), &buf) == -1)
        {
            throw CABRTException(EXCEP_PLUGIN, ssprintf("Can't stat archive file '%s'", pFilename));
        }
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw CABRTException(EXCEP_PLUGIN, "Curl library init error");
        }
        /* enable uploading */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        /* specify target */
        curl_easy_setopt(curl, CURLOPT_URL, wholeURL.c_str());
        curl_easy_setopt(curl, CURLOPT_READDATA, f);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, buf.st_size);
        /* everything is done here; result 0 means success */
        result = curl_easy_perform(curl);
        /* goodbye */
        curl_easy_cleanup(curl);
        fclose(f);
        if (result != 0)
        {
            update_client(_("Sending failed, trying again. %s"), curl_easy_strerror((CURLcode)result));
        }
    }
    /*retry the upload if not succesful, wait a bit before next try*/
    while (result != 0 && --count != 0 && (sleep(m_nRetryDelay), 1));

    if (count <= 0 && result != 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "Curl can not send a ticket");
    }
}


string CTicketUploader::Report(const map_crash_report_t& pCrashReport,
                               const map_plugin_settings_t& pSettings, const string& pArgs)
{
    update_client(_("Creating an TicketUploader report..."));

    // Get ticket name, customer name, and do_encrypt from config settings
    string customer_name = m_sCustomer;
    string ticket_name = m_sTicket;
    string upload_url = m_sURL;
    bool do_encrypt = m_bEncrypt;
    bool do_upload = m_bUpload;

    bool have_ticket_name = false;
    if (ticket_name == "")
    {
        ticket_name = "TicketUploader-newticket";
    }
    else
    {
        have_ticket_name = true;
    }

    // Format the time to add to the file name
    const int timebufmax = 256;
    char timebuf[timebufmax];
    time_t curtime = time(NULL);
    if (!strftime(timebuf, timebufmax, "-%G%m%d%k%M%S", gmtime(&curtime)))
    {
        Error("Can't format time");
    }

    // Create a tmp work directory, and within that the directory
    //   that will be the root of the tarball
    string file_name = ticket_name + timebuf;

    char tmpdir_name[] = "/tmp/rhuploadXXXXXX";
    if (mkdtemp(tmpdir_name) == NULL)
    {
        Error("Can't mkdir a temporary directory in /tmp");
    }
    string tmptar_name = concat_path_file(tmpdir_name, file_name.c_str());

    if (mkdir(tmptar_name.c_str(),S_IRWXU))
    {
        Error(ssprintf("Can't mkdir '%s'", tmptar_name.c_str()).c_str());
    }

    // Copy each entry into the tarball root,
    //   files are simply copied, strings are written to a file
    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT
         || it->second[CD_TYPE] == CD_ATT
        ) {
            string ofile_name = concat_path_file(tmptar_name.c_str(), it->first.c_str());
            ofstream ofile(ofile_name.c_str(), fstream::trunc|fstream::binary);
            if (!ofile)
            {
                Error(ssprintf("Can't open '%s'", ofile_name.c_str()).c_str());
            }
            ofile << it->second[CD_CONTENT] << endl;
            ofile.close();
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            string ofile_name = concat_path_file(tmptar_name.c_str(), it->first.c_str());
            if (copy_file(it->second[CD_CONTENT].c_str(), ofile_name.c_str()) < 0)
            {
                throw CABRTException(EXCEP_PLUGIN,
                        ssprintf("Can't copy '%s' to '%s'",
                                it->second[CD_CONTENT].c_str(),
                                ofile_name.c_str()
                        )
                );
            }
        }
    }

    // add ticket_name and customer name to tarball
    if (have_ticket_name)
    {
        string ofile_name = tmptar_name + "/TICKET";
        ofstream ofile(ofile_name.c_str(), fstream::trunc|fstream::binary);
        if (!ofile)
        {
            Error(ssprintf("Can't open '%s'", ofile_name.c_str()).c_str());
        }
        ofile << ticket_name << endl;
        ofile.close();
    }
    if (customer_name != "")
    {
        string ofile_name = tmptar_name + "/CUSTOMER";
        ofstream ofile(ofile_name.c_str(), fstream::trunc|fstream::binary);
        if (!ofile)
        {
            Error(ssprintf("Can't open '%s'", ofile_name.c_str()).c_str());
        }
        ofile << customer_name << endl;
        ofile.close();
    }

    // Create the compressed tarball
    string outfile_basename = file_name + ".tar.gz";
    string outfile_name = concat_path_file(tmpdir_name, outfile_basename.c_str());
    string cmd = ssprintf("tar -C %s --create --gzip --file=%s %s", tmpdir_name, outfile_name.c_str(), file_name.c_str());
    RunCommand(cmd.c_str());

    // encrypt if requested
    string key;
    if (do_encrypt)
    {
        key = ReadCommand("openssl rand -base64 48");

        string infile_name = outfile_name;
        outfile_basename += ".aes";
        outfile_name += ".aes";

        cmd = ssprintf("openssl aes-128-cbc -in %s -out %s -pass stdin", infile_name.c_str(), outfile_name.c_str());
        WriteCommand(cmd.c_str(), key.c_str());
    }

    // generate md5sum
    cmd = ssprintf("cd %s; md5sum %s", tmpdir_name, outfile_basename.c_str());
    string md5sum = ReadCommand(cmd.c_str());

    // upload or cp to /tmp
    if (do_upload)
    {
        // FIXME: SendFile isn't working sometime (scp)
        SendFile(upload_url.c_str(), outfile_name.c_str());
    }
    else
    {
        cmd = ssprintf("cp %s /tmp/", outfile_name.c_str());
        RunCommand(cmd.c_str());
    }

    // generate a reciept telling md5sum and encryption key
    // note: do not internationalize these strings!
    string msg;
    if (have_ticket_name)
    {
        msg += "Please copy this into ticket: ";
        msg += ticket_name;
        msg += '\n';
    }
    else
    {
        msg += "Please send this to your technical support:\n";
    }
    if (do_upload)
    {
        msg += "RHUPLOAD: This report was sent to ";
        msg += upload_url;
        msg += '\n';
    }
    else
    {
        msg += "RHUPLOAD: This report was copied into /tmp/:\n";
    }
    if (have_ticket_name)
    {
        msg += "TICKET: ";
        msg += ticket_name;
        msg += '\n';
    }
    msg += "FILE: ";
    msg += outfile_basename;
    msg += "\nMD5SUM:\n";
    msg += md5sum;
    if (do_encrypt)
    {
        msg += "KEY: aes-128-cbc\n";
        msg += key;
    }
    msg += "END:\n";

    /* warn the client: */
    error_msg("%s", msg.c_str());

    string ret;
    if (do_upload)
    {
        string xx = _("report sent to ") + upload_url + '/' + outfile_basename;
        update_client("%s", xx.c_str());
        ret = xx;
    }
    else
    {
        string xx = _("report copied to /tmp/") + outfile_basename;
        update_client("%s", xx.c_str());
        ret = xx;
    }

    // delete the temporary directory
    cmd = ssprintf("rm -rf %s", tmpdir_name);
    RunCommand(cmd.c_str());

    return ret;
}

void CTicketUploader::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("Customer");
    if (it != end)
    {
        m_sCustomer = it->second;
    }
    it = pSettings.find("Ticket");
    if (it != end)
    {
        m_sTicket = it->second;
    }
    it = pSettings.find("URL");
    if (it != end)
    {
        m_sURL = it->second;
    }
    it = pSettings.find("Encrypt");
    if (it != end)
    {
        m_bEncrypt = string_to_bool(it->second.c_str());
    }
    it = pSettings.find("Upload");
    if (it != end)
    {
        m_bUpload = string_to_bool(it->second.c_str());
    }
    it = pSettings.find("RetryCount");
    if (it != end)
    {
        m_nRetryCount = atoi(it->second.c_str());
    }
    it = pSettings.find("RetryDelay");
    if (it != end)
    {
        m_nRetryDelay = atoi(it->second.c_str());
    }
}

//ok to delete?
//const map_plugin_settings_t& CTicketUploader::GetSettings()
//{
//    m_pSettings["Customer"] = m_sCustomer;
//    m_pSettings["Ticket"] = m_sTicket;
//    m_pSettings["URL"] = m_sURL;
//    m_pSettings["Encrypt"] = m_bEncrypt ? "yes" : "no";
//    m_pSettings["Upload"] = m_bEncrypt ? "yes" : "no";
//    m_pSettings["RetryCount"] = to_string(m_nRetryCount);
//    m_pSettings["RetryDelay"] = to_string(m_nRetryDelay);
//
//    return m_pSettings;
//}

PLUGIN_INFO(REPORTER,
            CTicketUploader,
            "TicketUploader",
            "0.0.1",
            "Asks ticket# and customer name from user, sends report to FTP",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/TicketUploader.GTKBuilder");
