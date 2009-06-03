/*
    FileTransfer.cpp

    Copyright (C) 2009  Daniel Novotny (dnovotny@redhat.com)
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

#include "FileTransfer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <curl/curl.h>

#include "DebugDump.h"
#include "ABRTException.h"
#include "PluginSettings.h"
#include "CommLayerInner.h"


using namespace std;
#define HBLEN 255
#define FILETRANSFER_DIRLIST DEBUG_DUMPS_DIR "/FileTransferDirlist.txt"

/* FILETRANSFER_DIRLIST */

void CFileTransfer::SendFile(const std::string& pURL,
                             const std::string& pFilename)
{
    FILE * f;
    struct stat buf;
    CURL * curl;
    std::string wholeURL, protocol;
    int result, i, count = m_nRetryCount;
    int len = pURL.length();

    if (pURL == "")
    {
        comm_layer_inner_warning("FileTransfer: URL not specified");
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

    comm_layer_inner_status("Sending archive " + pFilename + " via " + protocol);

    if( pURL[len-1] == '/' )
    {
        wholeURL = pURL + pFilename;
    }
    else
    {
        wholeURL = pURL + "/" + pFilename;
    }

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
    }
    /*retry the upload if not succesful, wait a bit before next try*/
    while( result!=0 && count-- != 0 && (sleep(m_nRetryDelay),1) );

}

void CFileTransfer::CreateArchive(const std::string& pArchiveName,
                                  const std::string& pDir)
{
    std::string cmdline;
    int result;

    comm_layer_inner_status("Creating an archive...");

    /*TODO: consider library for archive creation, if there is any*/

    if(m_sArchiveType == ".tar.gz")
    {
       cmdline = "tar czf " + pArchiveName + " " + pDir;
    }
    else if(m_sArchiveType == ".tar.bz2")
    {
       cmdline = "tar cjf " + pArchiveName + " " + pDir;
    }
    else if(m_sArchiveType == ".zip")
    {
       cmdline = "zip " + pArchiveName + " " + pDir + "/*";
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::CreateArchive(): unknown/unsupported archive type "+m_sArchiveType);
    }

    comm_layer_inner_debug(cmdline);
    result = system(cmdline.c_str());

    if( result != 0 )
    {
        throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::CreateArchive(): Error while executing the archiver command.");
    }
}

/*returns the last component of the directory path*/
std::string CFileTransfer::DirBase(const std::string& pStr)
{
    std::string result;
    int i;

    i = pStr.length() - 1;
    if(pStr[i] == '/')
    {
        i--;
    }
    result="";
    for(; pStr[i] != '/'; i--)
    {
      result = pStr[i] + result;
    }
    return result;
}

void CFileTransfer::Run(const std::string& pActiveDir, const std::string& pArgs)
{
    fstream dirlist;

    comm_layer_inner_status("File Transfer: Creating a report...");

    if (pArgs == "store")
    {
        /* store pActiveDir for later sending */
        dirlist.open(FILETRANSFER_DIRLIST, fstream::out | fstream::app );
        dirlist << pActiveDir << endl;
        dirlist.close();
    }
    else
    {
        std::string dirname, archivename;

        char hostname[HBLEN];

        gethostname(hostname,HBLEN);

        dirlist.open(FILETRANSFER_DIRLIST, fstream::in);
        if(dirlist.fail())
        {
            /* this means there are no reports to send (no crashes, hurray)
               which is perfectly OK */
            return;
        }

        while(getline(dirlist,dirname), !dirlist.eof())
        {
            archivename = std::string(hostname) + "-"
                         + DirBase(dirname) + m_sArchiveType;
            try
            {
                CreateArchive(archivename,dirname);
                SendFile(m_sURL, archivename);
            }
            catch (CABRTException& e)
            {
                comm_layer_inner_warning("CFileTransfer::Run(): Cannot create and send an archive: " + e.what());
                comm_layer_inner_status("CFileTransfer::Run(): Cannot create and send an archive: " + e.what());
            }
            unlink(archivename.c_str());
        }

        dirlist.close();
        /* all the files we're able to send should be sent now,
           starting over with clean table */
        unlink(FILETRANSFER_DIRLIST);
    }
}

void CFileTransfer::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    plugin_load_settings(pPath, settings);

    if (settings.find("URL")!= settings.end())
    {
        m_sURL = settings["URL"];
    }
    else
    {
        comm_layer_inner_warning("FileTransfer: URL not specified");
    }

    if (settings.find("RetryCount")!= settings.end())
    {
        m_nRetryCount = atoi(settings["RetryCount"].c_str());
    }
    
    if (settings.find("RetryDelay")!= settings.end())
    {
        m_nRetryDelay = atoi(settings["RetryDelay"].c_str());
    }

    if (settings.find("ArchiveType")!= settings.end())
    {
        /* currently supporting .tar.gz, .tar.bz2 and .zip */
        m_sArchiveType = settings["ArchiveType"];
        if(m_sArchiveType[0] != '.')
        {
            m_sArchiveType =  "." + m_sArchiveType;
        }
    }

}
