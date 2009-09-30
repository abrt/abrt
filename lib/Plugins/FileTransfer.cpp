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

#define _GNU_SOURCE
#include <iostream>
#include <sstream>
#include <fstream>
#include<stdio.h>
#include<string.h>
#include<dirent.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<zip.h>
#include<libtar.h>
#include<bzlib.h>
#include<zlib.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "FileTransfer.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"


using namespace std;
#define HBLEN 255
#define FILETRANSFER_DIRLIST DEBUG_DUMPS_DIR "/FileTransferDirlist.txt"

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
        warn_client(_("FileTransfer: URL not specified"));

        return;
    }
    protocol = "";
    i = 0;
    while (pURL[i] != ':')
    {
        protocol += pURL[i];
        i++;
        if(i == len)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): malformed URL, does not contain protocol");
        }
    }

    char buffer[1024];
    snprintf(buffer, 1024, _("Sending archive %s via %s"), pFilename.c_str(), protocol.c_str());
    update_client(std::string(buffer));

    if (pURL[len-1] == '/')
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
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): cannot open archive file "+pFilename);
        }
        if (stat(pFilename.c_str(), &buf) == -1)
        {
            throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::SendFile(): cannot stat archive file "+pFilename);
        }
        curl = curl_easy_init();
        if (!curl)
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
    while (result != 0 && --count >= 0 && (sleep(m_nRetryDelay), 1));
}

/*
walks through the directory and applies a function with one
parameter "something" to each filename,
now used in create_zip, but can be useful for some future
archivers as well
*/
static void traverse_directory(const char * directory, void * something,
                               void (*func)(void *,const char *) )
{
    DIR * dp;
    struct dirent * dirp;
    char complete_name[BUFSIZ];
    char * end;

    dp = opendir(directory);
    if (dp == NULL)
    {
        return;
    }
    while ((dirp = readdir(dp)) != NULL)
    {
        if (is_regular_file(dirp, directory))
        {
            end = stpcpy(complete_name, directory);
            if (end[-1] != '/')
            {
                *end++ = '/';
            }
            end = stpcpy(end, dirp->d_name);

            func(something, complete_name);
        }
    }
    closedir(dp);
}

static void add_to_zip(void * z, const char * filename)
{
    struct zip_source *s;

    s = zip_source_file( (struct zip *)z, filename, 0, 0);
    zip_add( (struct zip *)z, filename, s);
}


static void create_zip(const char * archive_name, const char * directory)
{
    struct zip * z;

    z = zip_open(archive_name, ZIP_CREATE, NULL);
    if (z == NULL)
    {
        return;
    }
    traverse_directory(directory, z, add_to_zip);
    zip_close(z);
}

static void create_tar(const char * archive_name, const char * directory)
{
    TAR *tar;

    tar_open(&tar, (char *)archive_name, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU);
    tar_append_tree(tar, (char *)directory, ".");
    tar_close(tar);
}

static void create_targz(const char * archive_name, const char * directory)
{
    char * name_without_gz;
    char buf[BUFSIZ];
    FILE * f;
    ssize_t bytesRead;
    gzFile gz;

    name_without_gz = strdup(archive_name);
    strrchr(name_without_gz,'.')[0] = '\0';
    create_tar(name_without_gz, directory);

    f = fopen(name_without_gz, "r");
    if (f == NULL)
    {
        return;
    }
    gz = gzopen(archive_name, "w");
    if (gz == NULL)
    {
        fclose(f);
        return;
    }

    while ((bytesRead = fread(buf, 1, BUFSIZ, f)) > 0)
    {
        gzwrite(gz, buf, bytesRead);
    }
    gzclose(gz);
    fclose(f);
    remove(name_without_gz);
    free(name_without_gz);
}

static void create_tarbz2(const char * archive_name, const char * directory)
{
    char * name_without_bz2;
    char buf[BUFSIZ];
    FILE * f;
    ssize_t bytesRead;
    int tarFD;
    int bzError;
    BZFILE * bz;
#define BLOCK_MULTIPLIER 7

    name_without_bz2 = strdup(archive_name);
    strrchr(name_without_bz2,'.')[0] = '\0';
    create_tar(name_without_bz2, directory);

    tarFD = open(name_without_bz2, O_RDONLY);
    if (tarFD == -1)
    {
        return;
    }
    f = fopen(archive_name, "w");
    if (f == NULL)
    {
        close(tarFD);
        return;
    }
    bz = BZ2_bzWriteOpen(&bzError, f, BLOCK_MULTIPLIER, 0, 0);
    if (bz == NULL)
    {
        close(tarFD);
        fclose(f);
        return;
    }

    while ((bytesRead = read(tarFD,buf,BUFSIZ)) > 0)
    {
        BZ2_bzWrite(&bzError, bz, buf, bytesRead);
    }
    BZ2_bzWriteClose(&bzError, bz, 0, NULL, NULL);

    close(tarFD);
    fclose(f);
    remove(name_without_bz2);
    free(name_without_bz2);
}

void CFileTransfer::CreateArchive(const std::string& pArchiveName,
                                  const std::string& pDir)
{
    if (m_sArchiveType == ".tar")
    {
        create_tar(pArchiveName.c_str(), pDir.c_str());
    }
    else if (m_sArchiveType == ".tar.gz")
    {
        create_targz(pArchiveName.c_str(), pDir.c_str());
    }
    else if (m_sArchiveType == ".tar.bz2")
    {
        create_tarbz2(pArchiveName.c_str(), pDir.c_str());
    }
    else if (m_sArchiveType == ".zip")
    {
        create_zip(pArchiveName.c_str(), pDir.c_str());
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CFileTransfer::CreateArchive(): unknown/unsupported archive type "+m_sArchiveType);
    }
}

/*returns the last component of the directory path*/
std::string CFileTransfer::DirBase(const std::string& pStr)
{
    std::string result;
    int i;

    i = pStr.length() - 1;
    if (pStr[i] == '/')
    {
        i--;
    }
    result="";
    for (; pStr[i] != '/'; i--)
    {
      result = pStr[i] + result;
    }
    return result;
}

void CFileTransfer::Run(const std::string& pActiveDir, const std::string& pArgs)
{
    fstream dirlist;
    std::string dirname, archivename;
    char hostname[HBLEN];

    update_client(_("File Transfer: Creating a report..."));

    if (pArgs == "store")
    {
        /* store pActiveDir for later sending */
        dirlist.open(FILETRANSFER_DIRLIST, fstream::out | fstream::app );
        dirlist << pActiveDir << endl;
        dirlist.close();
    }
    else if (pArgs == "one")
    {
        /* just send one archive */
        gethostname(hostname,HBLEN);
        archivename = std::string(hostname) + "-"
                      + DirBase(pActiveDir) + m_sArchiveType;
        try
        {
            CreateArchive(archivename,pActiveDir);
            SendFile(m_sURL, archivename);
        }
        catch (CABRTException& e)
        {
            warn_client(_("CFileTransfer::Run(): Cannot create and send an archive: ") + e.what());
            //update_client("CFileTransfer::Run(): Cannot create and send an archive: " + e.what());
        }
        unlink(archivename.c_str());
    }
    else
    {
        gethostname(hostname,HBLEN);

        dirlist.open(FILETRANSFER_DIRLIST, fstream::in);
        if (dirlist.fail())
        {
            /* this means there are no reports to send (no crashes, hurray)
               which is perfectly OK */
            return;
        }

        while (getline(dirlist,dirname), !dirlist.eof())
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
                warn_client(_("CFileTransfer::Run(): Cannot create and send an archive: ") + e.what());
//                update_client("CFileTransfer::Run(): Cannot create and send an archive: " + e.what());
            }
            unlink(archivename.c_str());
        }

        dirlist.close();
        /* all the files we're able to send should be sent now,
           starting over with clean table */
        unlink(FILETRANSFER_DIRLIST);
    }
}

void CFileTransfer::SetSettings(const map_plugin_settings_t& pSettings)
{
    if (pSettings.find("URL") != pSettings.end())
    {
        m_sURL = pSettings.find("URL")->second;
    }
    else
    {
        warn_client(_("FileTransfer: URL not specified"));
    }

    if (pSettings.find("RetryCount") != pSettings.end())
    {
        m_nRetryCount = atoi(pSettings.find("RetryCount")->second.c_str());
    }

    if (pSettings.find("RetryDelay") != pSettings.end())
    {
        m_nRetryDelay = atoi(pSettings.find("RetryDelay")->second.c_str());
    }

    if (pSettings.find("ArchiveType") != pSettings.end())
    {
        /* currently supporting .tar, .tar.gz, .tar.bz2 and .zip */
        m_sArchiveType = pSettings.find("ArchiveType")->second;
        if(m_sArchiveType[0] != '.')
        {
            m_sArchiveType =  "." + m_sArchiveType;
        }
    }
}

map_plugin_settings_t CFileTransfer::GetSettings()
{
    map_plugin_settings_t ret;
    std::stringstream ss;
    ret["URL"] = m_sURL;
    ss << m_nRetryCount;
    ret["RetryCount"] = ss.str();
    ss.str("");
    ss << m_nRetryDelay;
    ret["RetryDelay"] = ss.str();
    ret["ArchiveType"] = m_sArchiveType;

    return ret;
}

PLUGIN_INFO(ACTION,
            CFileTransfer,
            "FileTransfer",
            "0.0.6",
            "Sends a report via FTP or SCTP",
            "dnovotny@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
