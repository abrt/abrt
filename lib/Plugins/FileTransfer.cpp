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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zip.h>
#include <libtar.h>
#include <bzlib.h>
#include <zlib.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "FileTransfer.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"

using namespace std;

#define HBLEN 255
#define FILETRANSFER_DIRLIST DEBUG_DUMPS_DIR "/FileTransferDirlist.txt"

CFileTransfer::CFileTransfer()
:
    m_sArchiveType(".tar.gz"),
    m_nRetryCount(3),
    m_nRetryDelay(20)
{
}

void CFileTransfer::SendFile(const char *pURL, const char *pFilename)
{
    int len = strlen(pURL);
    if (len == 0)
    {
        error_msg(_("FileTransfer: URL not specified"));
        return;
    }

    update_client(_("Sending archive %s to %s"), pFilename, pURL);

    string wholeURL = concat_path_file(pURL, pFilename);

    int count = m_nRetryCount;
    while (1)
    {
        FILE *f;
        struct stat buf;
        CURL *curl;

        f = fopen(pFilename, "r");
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "Can't open archive file '%s'", pFilename);
        }
        if (fstat(fileno(f), &buf) == -1)
        {
            fclose(f);
            throw CABRTException(EXCEP_PLUGIN, "Can't stat archive file '%s'", pFilename);
        }
        curl = curl_easy_init();
        if (!curl)
        {
            throw CABRTException(EXCEP_PLUGIN, "Curl library init error");
        }
        /* enable uploading */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        /* specify target */
        curl_easy_setopt(curl, CURLOPT_URL, wholeURL.c_str());
        /* FILE handle: passed to the default callback, it will fread() it */
        curl_easy_setopt(curl, CURLOPT_READDATA, f);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, buf.st_size);
        /* everything is done here; result 0 means success */
        int result = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(f);
        if (result == 0 || --count <= 0)
            break;
        /* retry the upload if not succesful, wait a bit before next try */
        sleep(m_nRetryDelay);
    }
}

/*
walks through the directory and applies a function with one
parameter "something" to each filename,
now used in create_zip, but can be useful for some future
archivers as well
*/
static void traverse_directory(const char * directory, void * something,
                               void (*func)(void *, const char *) )
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

    if (tar_open(&tar, (char *)archive_name, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU) != 0)
    {
        return;
    }
    tar_append_tree(tar, (char *)directory, (char*)".");
    tar_close(tar);
}

static void create_targz(const char * archive_name, const char * directory)
{
    char * name_without_gz;
    char buf[BUFSIZ];
    FILE * f;
    ssize_t bytesRead;
    gzFile gz;

    name_without_gz = xstrdup(archive_name);
    strrchr(name_without_gz, '.')[0] = '\0';
    create_tar(name_without_gz, directory);

    f = fopen(name_without_gz, "r");
    if (f == NULL)
    {
        free(name_without_gz);
        return;
    }
    gz = gzopen(archive_name, "w");
    if (gz == NULL)
    {
        fclose(f);
        free(name_without_gz);
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

    name_without_bz2 = xstrdup(archive_name);
    strrchr(name_without_bz2, '.')[0] = '\0';
    create_tar(name_without_bz2, directory);

    tarFD = open(name_without_bz2, O_RDONLY);
    if (tarFD == -1)
    {
        free(name_without_bz2);
        return;
    }
    f = fopen(archive_name, "w");
    if (f == NULL)
    {
        close(tarFD);
        free(name_without_bz2);
        return;
    }
    bz = BZ2_bzWriteOpen(&bzError, f, BLOCK_MULTIPLIER, 0, 0);
    if (bz == NULL)
    {
        close(tarFD);
        fclose(f);
        free(name_without_bz2);
        return;
    }

    while ((bytesRead = read(tarFD, buf, BUFSIZ)) > 0)
    {
        BZ2_bzWrite(&bzError, bz, buf, bytesRead);
    }
    BZ2_bzWriteClose(&bzError, bz, 0, NULL, NULL);

    close(tarFD);
    fclose(f);
    remove(name_without_bz2);
    free(name_without_bz2);
}

void CFileTransfer::CreateArchive(const char *pArchiveName, const char *pDir)
{
    if (m_sArchiveType == ".tar")
    {
        create_tar(pArchiveName, pDir);
    }
    else if (m_sArchiveType == ".tar.gz")
    {
        create_targz(pArchiveName, pDir);
    }
    else if (m_sArchiveType == ".tar.bz2")
    {
        create_tarbz2(pArchiveName, pDir);
    }
    else if (m_sArchiveType == ".zip")
    {
        create_zip(pArchiveName, pDir);
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "Unknown/unsupported archive type %s", m_sArchiveType.c_str());
    }
}

/* Returns the last component of the directory path.
 * Careful to not return "" on "/path/path2/", but "path2".
 */
static string DirBase(const char *pStr)
{
    int end = strlen(pStr);
    if (end > 1 && pStr[end-1] == '/')
    {
        end--;
    }
    int beg = end;
    while (beg > 0 && pStr[beg-1] != '/')
    {
        beg--;
    }
    return string(pStr + beg, end - beg);
}

void CFileTransfer::Run(const char *pActionDir, const char *pArgs)
{
    update_client(_("File Transfer: Creating a report..."));

    char hostname[HBLEN];
    gethostname(hostname, HBLEN-1);
    hostname[HBLEN-1] = '\0';

    fstream dirlist;
    if (strcmp(pArgs, "store") == 0)
    {
        /* store pActiveDir for later sending */
        dirlist.open(FILETRANSFER_DIRLIST, fstream::out | fstream::app);
        dirlist << pActionDir << endl;
        dirlist.close();
    }
    else if (strcmp(pArgs, "one") == 0)
    {
        /* just send one archive */
        string archivename = ssprintf("%s-%s%s", hostname, DirBase(pActionDir).c_str(), m_sArchiveType.c_str());
        try
        {
            CreateArchive(archivename.c_str(), pActionDir);
            SendFile(m_sURL.c_str(), archivename.c_str());
        }
        catch (CABRTException& e)
        {
            error_msg(_("Can't create and send an archive: %s"), e.what());
        }
        unlink(archivename.c_str());
    }
    else
    {
        dirlist.open(FILETRANSFER_DIRLIST, fstream::in);
        if (dirlist.fail())
        {
            /* this means there are no reports to send (no crashes, hurray)
               which is perfectly OK */
            return;
        }

        string dirname;
        while (getline(dirlist, dirname), dirlist.good())
        {
            string archivename = ssprintf("%s-%s%s", hostname, DirBase(dirname.c_str()).c_str(), m_sArchiveType.c_str());
            try
            {
                CreateArchive(archivename.c_str(), dirname.c_str());
                SendFile(m_sURL.c_str(), archivename.c_str());
            }
            catch (CABRTException& e)
            {
                error_msg(_("Can't create and send an archive %s"), e.what());
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
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("URL");
    if (it != end)
    {
        m_sURL = it->second;
    }
    else
    {
        error_msg(_("FileTransfer: URL not specified"));
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

    it = pSettings.find("ArchiveType");
    if (it != end)
    {
        /* currently supporting .tar, .tar.gz, .tar.bz2 and .zip */
        m_sArchiveType = it->second;
        if (m_sArchiveType[0] != '.')
        {
            m_sArchiveType = "." + m_sArchiveType;
        }
    }
}

//ok to delete?
//const map_plugin_settings_t& CFileTransfer::GetSettings()
//{
//    m_pSettings["URL"] = m_sURL;
//    m_pSettings["RetryCount"] = to_string(m_nRetryCount);
//    m_pSettings["RetryDelay"] = to_string(m_nRetryDelay);
//    m_pSettings["ArchiveType"] = m_sArchiveType;
//
//    return m_pSettings;
//}

PLUGIN_INFO(ACTION,
            CFileTransfer,
            "FileTransfer",
            "0.0.6",
            "Sends a report via FTP or SCTP",
            "dnovotny@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
