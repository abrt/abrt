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

#include "TicketUploader.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

#include <stdlib.h>
#include <sys/stat.h>

#include <string>
#include <fstream>
#include <sstream>
#include <ext/stdio_filebuf.h>
#include <curl/curl.h>


CTicketUploader::CTicketUploader() :
    m_sCustomer(""),
    m_sTicket(""),
    m_sURL(""),
    m_bEncrypt(false),
    m_bUpload(false),
    m_nRetryCount(3),
    m_nRetryDelay(20)
{}

CTicketUploader::~CTicketUploader()
{}



void CTicketUploader::Error(string func, string msg)
{
    update_client(msg);
    throw CABRTException(EXCEP_PLUGIN, func + msg);
}

void CTicketUploader::CopyFile(const std::string& pSourceName, const std::string& pDestName)
{
    std::ifstream source(pSourceName.c_str(), std::fstream::binary);

    if (!source)
    {
        throw CABRTException(EXCEP_PLUGIN, "CActionSOSreport::CopyFile(): could not open input sosreport filename:" + pSourceName);
    }
    std::ofstream dest(pDestName.c_str(),std::fstream::trunc|std::fstream::binary);
    if (!dest)
    {
        throw CABRTException(EXCEP_PLUGIN, "CActionSOSreport::CopyFile(): could not open output sosreport filename:" + pDestName);
    }
    dest << source.rdbuf();
}

void CTicketUploader::RunCommand(string cmd)
{
    int retcode = system(cmd.c_str());
    if (retcode == -1)
    {
        Error("TicketUploader::RunCommand:", "error: could not start subshell: " + cmd);
    }
    if (retcode)
    {
        std::ostringstream msg;
        msg << "error: subshell failed (rc=" << retcode << "):" << cmd;
        Error("TicketUploader::RunCommand:", msg.str());
    }
}

string CTicketUploader::ReadCommand(string cmd)
{
    FILE* fp = popen(cmd.c_str(),"r");
    if (!fp)
    {
        Error("TicketUploader::ReadCommand:", "error: could not start subshell: " + cmd);
    }

    __gnu_cxx::stdio_filebuf<char> command_output_buffer(fp, std::ios_base::in);
    std::ostringstream output_stream;
    output_stream << &command_output_buffer;

    int retcode = pclose(fp);
    if (retcode)
    {
        std::ostringstream msg;
        msg << "error: subshell failed (rc=" << retcode << "):" << cmd;
        Error("TicketUploader::ReadCommand:", msg.str());
    }

    return output_stream.str();
}

void CTicketUploader::WriteCommand(string cmd,string input)
{
    FILE* fp = popen(cmd.c_str(),"w");
    if (!fp)
    {
        Error("TicketUploader::WriteCommand:", "error: could not start subshell: " + cmd);
    }

    size_t input_length = input.length();
    size_t check = fwrite(input.c_str(),1,input_length,fp);
    if (input_length != check)
    {
        Error("TicketUploader::WriteCommand:", "error: could not send input to subshell: " + cmd);
    }

    int retcode = pclose(fp);
    if (retcode)
    {
        std::ostringstream msg;
        msg << "error: subshell failed (rc=" << retcode << "):" << cmd;
        Error("TicketUploader::ReadCommand:", msg.str());
    }

}

void CTicketUploader::SendFile(const std::string& pURL,
                               const std::string& pFilename)
{
    FILE * f;
    struct stat buf;
    CURL * curl;
    std::string wholeURL, protocol;
    int result, i, count = m_nRetryCount;
    int len = pURL.length();
    std::string file;

    if (pURL == "")
    {
        warn_client(_("FileTransfer: URL not specified"));
        return;
    }
    protocol = "";
    i = 0;
    while(pURL[i] != ':')
    {
        protocol += pURL[i];
        i++;
        if(i == len)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): malformed URL, does not contain protocol");
        }
    }

    file = pFilename.substr(pFilename.rfind("/") + 1, pFilename.length());

    if( pURL[len-1] == '/' )
    {
        wholeURL = pURL + file;
    }
    else
    {
        wholeURL = pURL + "/" + file;
    }

    update_client(_("Sending archive ") + pFilename + _(" via ") + protocol + _(" to ") + pURL);

    do
    {
        f = fopen(pFilename.c_str(),"r");
        if(!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): cannot open archive file "+pFilename);
        }
        if (stat(pFilename.c_str(), &buf) == -1)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): cannot stat archive file "+pFilename);
        }
        curl = curl_easy_init();
        if(!curl)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): Curl library error.");
        }
        /* enable uploading */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        /* specify target */
        curl_easy_setopt(curl, CURLOPT_URL, wholeURL.c_str());
        /*file handle: passed to the default callback, it will fread() it*/
        curl_easy_setopt(curl, CURLOPT_READDATA, f);
        /*get file size*/
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, buf.st_size);
        /*everything is done here; result 0 means success*/
        result = curl_easy_perform(curl);
        /*goodbye*/
        curl_easy_cleanup(curl);
        fclose(f);
        if (result != 0)
        {
            update_client(_("Sending failed, try it again: ") + std::string(curl_easy_strerror((CURLcode)result)));
        }
    }
    /*retry the upload if not succesful, wait a bit before next try*/
    while( result!=0 && --count != 0 && (sleep(m_nRetryDelay),1) );

    if (count <= 0 && result != 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): Curl can not send a ticket.");
    }
}


string CTicketUploader::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    string ret;
    update_client(_("Creating an TicketUploader report..."));



    // Get ticket name, customer name, and do_encrypt from config settings
    string ticket_name;
    string customer_name;
    string upload_url;
    bool do_encrypt = false;
    bool do_upload = false;

    customer_name = m_sCustomer;
    ticket_name = m_sTicket;
    upload_url = m_sURL;
    do_encrypt = m_bEncrypt;
    do_upload = m_bUpload;

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
    if (!strftime(timebuf,timebufmax,"-%G%m%d%k%M%S",gmtime(&curtime)))
    {
        Error("TicketUploader::Report:","could not format time");
    }



    // Create a tmp work directory, and within that the directory
    //   that will be the root of the tarball
    string file_name = ticket_name + timebuf;

    char TEMPLATE[] = "/tmp/rhuploadXXXXXX";
    string tmpdir_name = mkdtemp(TEMPLATE);
    string tmptar_name = tmpdir_name + '/' + file_name;

    if (mkdir(tmptar_name.c_str(),S_IRWXU))
    {
        Error("TicketUploader::Report:","error: could not mkdir: " + tmptar_name);
    }



    // Copy each entry into the tarball root,
    //   files are simply copied, strings are written to a file
    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            string ofile_name = tmptar_name + '/' + it->first;
            std::ofstream ofile(ofile_name.c_str(),std::fstream::trunc|std::fstream::binary);
            if (!ofile)
            {
                Error("TicketUploader::Report:","error: could not open: " + ofile_name);
            }
            ofile << it->second[CD_CONTENT] << std::endl;
            ofile.close();
        }
        if (it->second[CD_TYPE] == CD_ATT)
        {
            string ofile_name = tmptar_name + '/' + it->first;
            std::ofstream ofile(ofile_name.c_str(),std::fstream::trunc|std::fstream::binary);
            if (!ofile)
            {
                Error("TicketUploader::Report:","error: could not open: " + ofile_name);
            }
            ofile << it->second[CD_CONTENT] << std::endl;
            ofile.close();
         }
        if (it->second[CD_TYPE] == CD_BIN)
        {
            string ofile_name = tmptar_name + '/' + it->first;
            CopyFile(it->second[CD_CONTENT],ofile_name);
        }
    }



    // add ticket_name and customer name to tarball
    if (have_ticket_name)
    {
        string ofile_name = tmptar_name + "/TICKET";
        std::ofstream ofile(ofile_name.c_str(),std::fstream::trunc|std::fstream::binary);
        if (!ofile)
        {
            Error("TicketUploader::Report:","error: could not open: " + ofile_name);
        }
        ofile << ticket_name << std::endl;
        ofile.close();
    }
    if (customer_name != "")
    {
        string ofile_name = tmptar_name + "/CUSTOMER";
        std::ofstream ofile(ofile_name.c_str(),std::fstream::trunc|std::fstream::binary);
        if (!ofile)
        {
            Error("TicketUploader::Report:","error: could not open: " + ofile_name);
        }
        ofile << customer_name << std::endl;
        ofile.close();
    }



    // Create the compressed tarball
    string outfile_basename = file_name + ".tar.gz";
    string outfile_name = tmpdir_name + '/' + outfile_basename;
    string cmd = string("tar -C ") + tmpdir_name +
      " --create --gzip --file=" + outfile_name + ' ' + file_name;
    RunCommand(cmd);




    // encrypt if requested
    string key;
    if (do_encrypt)
    {
        cmd = string("openssl rand -base64 48");
        key = ReadCommand(cmd);

        string infile_name = outfile_name;
        outfile_basename += ".aes";
        outfile_name += ".aes";

        cmd = string("openssl aes-128-cbc -in ") + infile_name +
          " -out " + outfile_name + " -pass stdin";
        WriteCommand(cmd,key);
    }



    // generate md5sum
    cmd = string("cd ") + tmpdir_name + string("; md5sum ") + outfile_basename;
    string md5sum = ReadCommand(cmd);



    // upload or cp to /tmp
    if (do_upload)
    {
        // FIXME: SendFile isn't working sometime (scp)
        SendFile(upload_url,outfile_name);
    }
    else
    {
        cmd = string("cp ") + outfile_name + " /tmp/";
        RunCommand(cmd);
    }



    // generate a reciept telling md5sum and encryption key
    std::ostringstream msgbuf;
    if (have_ticket_name)
        msgbuf << _("Please copy this into ticket: ") << ticket_name << std::endl;
    else
        msgbuf << _("Please send this to your technical support: ") << std::endl;
    if (do_upload)
        msgbuf << _("RHUPLOAD: This report was sent to ") + upload_url << std::endl;
    else
        msgbuf << _("RHUPLOAD: This report was copied into /tmp/: ") << std::endl;
    if (have_ticket_name)
        msgbuf << _("TICKET: ") << ticket_name << std::endl;
    msgbuf << _("FILE: ") << outfile_basename << std::endl;
    msgbuf << _("MD5SUM: ") << std::endl;
    msgbuf << md5sum;
    if (do_encrypt)
    {
        msgbuf << _("KEY: aes-128-cbc") << std::endl;
        msgbuf << key;
    }
    msgbuf << _("END: ") << std::endl;

    warn_client(msgbuf.str());

    if (do_upload)
    {
        string xx = _("report sent to ") + upload_url + '/' + outfile_basename;
        update_client(xx);
        ret = xx;
    }
    else
    {
        string xx = _("report copied to /tmp/") + outfile_basename;
        update_client(xx);
        ret = xx;
    }

    // delete the temporary directory
    cmd = string("rm -rf ") + tmpdir_name;
    RunCommand(cmd);

    return ret;
}

void CTicketUploader::SetSettings(const map_plugin_settings_t& pSettings)
{
    if (pSettings.find("Customer") != pSettings.end())
    {
        m_sCustomer = pSettings.find("Customer")->second;
    }
    if (pSettings.find("Ticket") != pSettings.end())
    {
        m_sTicket = pSettings.find("Ticket")->second;
    }
    if (pSettings.find("URL") != pSettings.end())
    {
        m_sURL = pSettings.find("URL")->second;
    }
    if (pSettings.find("Encrypt") != pSettings.end())
    {
        m_bEncrypt = pSettings.find("Encrypt")->second == "yes";
    }
    if (pSettings.find("Upload") != pSettings.end())
    {
        m_bUpload = pSettings.find("Upload")->second == "yes";
    }
    if (pSettings.find("RetryCount") != pSettings.end())
    {
        m_nRetryCount = atoi(pSettings.find("RetryCount")->second.c_str());
    }
    if (pSettings.find("RetryDelay") != pSettings.end())
    {
        m_nRetryDelay = atoi(pSettings.find("RetryDelay")->second.c_str());
    }
}

map_plugin_settings_t CTicketUploader::GetSettings()
{
    map_plugin_settings_t ret;

    ret["Customer"] = m_sCustomer;
    ret["Ticket"] = m_sTicket;
    ret["URL"] = m_sURL;
    ret["Encrypt"] = m_bEncrypt ? "yes" : "no";
    ret["Upload"] = m_bEncrypt ? "yes" : "no";

    std::stringstream ss;
    ss << m_nRetryCount;
    ret["RetryCount"] = ss.str();
    ss.str("");
    ss << m_nRetryDelay;
    ret["RetryDelay"] = ss.str();

    return ret;
}

PLUGIN_INFO(REPORTER,
            CTicketUploader,
            "TicketUploader",
            "0.0.1",
            "input ticket# and customer name from user, send report to FTP",
            "gavin@redhat.com",
            "https://fedorahosted.org/abtr/wiki",
            PLUGINS_LIB_DIR"/TicketUploader.GTKBuilder");
