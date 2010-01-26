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
#include "abrtlib.h"
#include "abrt_xmlrpc.h" /* for xcurl_easy_init */
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


static void RunCommand(const char *cmd)
{
    int retcode = system(cmd);
    if (retcode)
    {
        throw CABRTException(EXCEP_PLUGIN, "'%s' exited with %d", cmd, retcode);
    }
}

static string ReadCommand(const char *cmd)
{
    FILE* fp = popen(cmd, "r");
    if (!fp)
    {
        throw CABRTException(EXCEP_PLUGIN, "Error running '%s'", cmd);
    }

    string result;
    char buff[1024];
    while (fgets(buff, sizeof(buff), fp) != NULL)
    {
        strchrnul(buff, '\n')[0] = '\0';
        result += buff;
    }

    int retcode = pclose(fp);
    if (retcode)
    {
        throw CABRTException(EXCEP_PLUGIN, "'%s' exited with %d", cmd, retcode);
    }

    return result;
}

static void WriteCommand(const char *cmd, const char *input)
{
    FILE* fp = popen(cmd, "w");
    if (!fp)
    {
        throw CABRTException(EXCEP_PLUGIN, "error running '%s'", cmd);
    }

    /* Hoping it's not too big to get us forever blocked... */
    fputs(input, fp);

    int retcode = pclose(fp);
    if (retcode)
    {
        throw CABRTException(EXCEP_PLUGIN, "'%s' exited with %d", cmd, retcode);
    }
}

void CTicketUploader::SendFile(const char *pURL, const char *pFilename, int retry_count, int retry_delay)
{
    if (pURL[0] == '\0')
    {
        error_msg(_("FileTransfer: URL not specified"));
        return;
    }

    update_client(_("Sending archive %s to %s"), pFilename, pURL);

    const char *base = (strrchr(pFilename, '/') ? : pFilename-1) + 1;
    string wholeURL = concat_path_file(pURL, base);
    int count = retry_count;
    int result;
    while (1)
    {
        FILE* f = fopen(pFilename, "r");
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "Can't open archive file '%s'", pFilename);
        }
        struct stat buf;
        fstat(fileno(f), &buf); /* never fails */
        CURL* curl = xcurl_easy_init();
        /* enable uploading */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        /* specify target */
        curl_easy_setopt(curl, CURLOPT_URL, wholeURL.c_str());
        curl_easy_setopt(curl, CURLOPT_READDATA, f);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)buf.st_size);
        /* everything is done here; result 0 means success */
        result = curl_easy_perform(curl);
        /* goodbye */
        curl_easy_cleanup(curl);
        fclose(f);
        if (result != 0)
        {
            update_client(_("Sending failed, trying again. %s"), curl_easy_strerror((CURLcode)result));
        }
        if (result == 0 || --count <= 0)
            break;
        /* retry the upload if not succesful, wait a bit before next try */
        sleep(retry_delay);
    }

    if (count <= 0 && result != 0)
    {
        throw CABRTException(EXCEP_PLUGIN, "Curl can not send a ticket");
    }
}


static void write_str_to_file(const char *str, const char *path, const char *fname)
{
    string ofile_name = concat_path_file(path, fname);
    FILE *ofile = fopen(ofile_name.c_str(), "w");
    if (!ofile)
    {
        throw CABRTException(EXCEP_PLUGIN, "Can't open '%s'", ofile_name.c_str());
    }
    fprintf(ofile, "%s\n", str);
    fclose(ofile);
}

string CTicketUploader::Report(const map_crash_data_t& pCrashData,
                const map_plugin_settings_t& pSettings,
                const char *pArgs)
{
    string customer_name;
    string ticket_name;
    string upload_url;
    bool do_encrypt;
    bool do_upload;
    int retry_count;
    int retry_delay;

    /* if parse_settings fails it returns an empty map so we need to use defaults */
    map_plugin_settings_t settings = parse_settings(pSettings);
    // Get ticket name, customer name, and do_encrypt from config settings
    if (!settings.empty())
    {
        customer_name = settings["Customer"];
        ticket_name = settings["Ticket"];
        upload_url = settings["URL"];
        do_encrypt = string_to_bool(settings["Encrypt"].c_str());
        do_upload =  string_to_bool(settings["Upload"].c_str());
        retry_count = xatoi_u(settings["RetryCount"].c_str());
        retry_delay = xatoi_u(settings["RetryDelay"].c_str());
    }
    else
    {
        customer_name = m_sCustomer;
        ticket_name = m_sTicket;
        upload_url = m_sURL;
        do_encrypt = m_bEncrypt;
        do_upload = m_bUpload;
        retry_count = m_nRetryCount;
        retry_delay = m_nRetryDelay;
    }
    update_client(_("Creating an TicketUploader report..."));

    bool have_ticket_name = (ticket_name != "");
    if (!have_ticket_name)
    {
        ticket_name = "TicketUploader-newticket";
    }

    // Format the time to add to the file name
    char timebuf[256];
    time_t curtime = time(NULL);
    strftime(timebuf, sizeof(timebuf), "-%Y%m%d%H%M%S", gmtime(&curtime));

    // Create a tmp work directory, and within that
    // create the "<ticketname>-yyyymmddhhmmss" directory
    // which will be the root of the tarball
    string file_name = ticket_name + timebuf;

    char tmpdir_name[] = "/tmp/abrtuploadXXXXXX";
    if (mkdtemp(tmpdir_name) == NULL)
    {
        throw CABRTException(EXCEP_PLUGIN, "Can't mkdir a temporary directory in /tmp");
    }
    string tmptar_name = concat_path_file(tmpdir_name, file_name.c_str());

    if (mkdir(tmptar_name.c_str(), 0700))
    {
        throw CABRTException(EXCEP_PLUGIN, "Can't mkdir '%s'", tmptar_name.c_str());
    }

    // Copy each entry into the tarball root.
    // Files are simply copied, strings are written to a file
    map_crash_data_t::const_iterator it;
    for (it = pCrashData.begin(); it != pCrashData.end(); it++)
    {
        const char *content = it->second[CD_CONTENT].c_str();
        if (it->second[CD_TYPE] == CD_TXT)
        {
            write_str_to_file(content, tmptar_name.c_str(), it->first.c_str());
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            string ofile_name = concat_path_file(tmptar_name.c_str(), it->first.c_str());
            if (copy_file(content, ofile_name.c_str(), 0644) < 0)
            {
                throw CABRTException(EXCEP_PLUGIN,
                        "Can't copy '%s' to '%s'",
                        content,
                        ofile_name.c_str()
                );
            }
        }
    }

    // add ticket_name and customer name to tarball
    if (have_ticket_name)
    {
        write_str_to_file(ticket_name.c_str(), tmptar_name.c_str(), "TICKET");
    }
    if (customer_name != "")
    {
        write_str_to_file(customer_name.c_str(), tmptar_name.c_str(), "CUSTOMER");
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
    cmd = ssprintf("cd %s; md5sum <%s", tmpdir_name, outfile_basename.c_str());
    string md5sum = ReadCommand(cmd.c_str());

    // upload or cp to /tmp
    if (do_upload)
    {
        // FIXME: SendFile isn't working sometime (scp)
        SendFile(upload_url.c_str(), outfile_name.c_str(), retry_count, retry_delay);
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
        msg += "========cut here========\n";
    }
    else
    {
        msg += "Please send this to your technical support:\n";
        msg += "========cut here========\n";
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
    msg += "\nMD5SUM: ";
    msg += md5sum;
    msg += '\n';
    if (do_encrypt)
    {
        msg += "KEY: aes-128-cbc\n";
        msg += key;
        msg += '\n';
    }
    msg += "==========end===========\n";

    // warn the client (why _warn_? it's not an error, maybe update_client?):
    //error_msg("%s", msg.c_str());

    // delete the temporary directory
    cmd = ssprintf("rm -rf %s", tmpdir_name);
    RunCommand(cmd.c_str());

    return msg;
}

static bool is_string_safe(const char *str)
{
    const char *p = str;
    while (*p)
    {
        unsigned char c = *p;
        if ((c < '0' || c > '9')
         && c != '_'
         && c != '-'
        ) {
            c |= 0x20; // tolower
            if (c < 'a' || c > 'z')
            {
                return false;
            }
        }
        // only 0-9, -, _, A-Z, a-z reach this point
        p++;
    }
    return true;
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
    // We use m_sTicket as part of filename,
    // and we use resulting filename in system("cd %s; ...", filename) etc,
    // so we are very paraniod about allowed chars
    it = pSettings.find("Ticket");
    if (it != end && is_string_safe(it->second.c_str()))
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
        m_nRetryCount = xatoi_u(it->second.c_str());
    }
    it = pSettings.find("RetryDelay");
    if (it != end)
    {
        m_nRetryDelay = xatoi_u(it->second.c_str());
    }
}

const map_plugin_settings_t& CTicketUploader::GetSettings()
{
    m_pSettings["Customer"] = m_sCustomer;
    m_pSettings["Ticket"] = m_sTicket;
    m_pSettings["URL"] = m_sURL;
    m_pSettings["Encrypt"] = m_bEncrypt ? "yes" : "no";
    m_pSettings["Upload"] = m_bUpload ? "yes" : "no";
    m_pSettings["RetryCount"] = to_string(m_nRetryCount);
    m_pSettings["RetryDelay"] = to_string(m_nRetryDelay);

    return m_pSettings;
}

//todo: make static
map_plugin_settings_t CTicketUploader::parse_settings(const map_plugin_settings_t& pSettings)
{
    map_plugin_settings_t plugin_settings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
   
    it = pSettings.find("Customer");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Customer"] = it->second;
    
    it = pSettings.find("Ticket");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Ticket"] = it->second;
    
    it = pSettings.find("URL");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["URL"] = it->second;
    
    it = pSettings.find("Encrypt");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Encrypt"] = it->second;
    
    it = pSettings.find("Upload");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Upload"] = it->second;
    
    it = pSettings.find("RetryCount");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["RetryCount"] = it->second;
    
    it = pSettings.find("RetryDelay");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["RetryDelay"] = it->second;
    
    VERB1 log("User settings ok, using them instead of defaults");
    return plugin_settings;
}

PLUGIN_INFO(REPORTER,
            CTicketUploader,
            "TicketUploader",
            "0.0.1",
            "Asks ticket# and customer name from user, sends report to FTP",
            "gavin@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/TicketUploader.GTKBuilder");
