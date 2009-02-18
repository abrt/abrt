/*
    Packages.cpp - PackageKit wrapper

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
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

#include "Packages.h"
#include <rpm/rpmcli.h>
#include <iostream>


CPackages::CPackages() :
    m_pPkClient(NULL),
    m_bBusy(false)
{
    g_type_init();
    m_pPkClient = pk_client_new();

    uint8_t* pkt;
    size_t pklen;
    pgpKeyID_t keyID;
    char *argv[] = {(char*)""};
    poptContext context = rpmcliInit(0, argv, NULL);

    // TODO: make this configurable

    pgpReadPkts("/etc/pki/rpm-gpg/RPM-GPG-KEY-fedora", &pkt, &pklen);
    if (pgpPubkeyFingerprint(pkt, pklen, keyID) == 0)
    {
        char* fedoraFingerprint = pgpHexStr(keyID, sizeof(keyID));
        if (fedoraFingerprint != NULL)
        {
            m_setFingerprints.insert(fedoraFingerprint);
        }
        free(pkt);
    }
    rpmcliFini(context);
}

CPackages::~CPackages()
{
    g_object_unref(m_pPkClient);
}

bool CPackages::CheckFingerprint(const Header& pHeader)
{
    if (!headerIsEntry(pHeader, RPMTAG_SIGGPG))
    {
        return false;
    }
    std::cout << "aaa" << std::endl;
    char* headerFingerprint;
    rpmtd td = rpmtdNew();
    headerGet(pHeader, RPMTAG_SIGGPG, td, HEADERGET_DEFAULT);
    headerFingerprint = pgpHexStr((const uint8_t*)td->data + 9, sizeof(pgpKeyID_t));
    rpmtdFree(td);
    if (headerFingerprint != NULL)
    {
        if (m_setFingerprints.find(headerFingerprint) == m_setFingerprints.end())
        {
            free(headerFingerprint);
            return false;
        }
        free(headerFingerprint);
        return true;
    }
    return false;
}

bool CPackages::CheckHash(const Header& pHeader, const rpmts& pTs, const std::string&pPath)
{
    rpmfi fi = rpmfiNew(pTs, pHeader, RPMTAG_BASENAMES, 0);
    pgpHashAlgo hashAlgo;
    std::string headerHash;
    char computedHash[1024] = "";

    while(rpmfiNext(fi) != -1)
    {
        if (pPath == rpmfiFN(fi))
        {
            headerHash = rpmfiFDigestHex(fi, &hashAlgo);
        }
    }
    rpmfiFree(fi);

    rpmDoDigest(hashAlgo, pPath.c_str(), 1, (unsigned char*) computedHash, NULL);

    if (headerHash == "" || std::string(computedHash) == "")
    {
        return false;
    }
    else if (headerHash == computedHash)
    {
        return true;
    }
    return false;
}

std::string CPackages::SearchFile(const std::string& pPath)
{
    std::string ret = "";
    char *argv[] = {(char*)""};
    poptContext context = rpmcliInit(0, argv, NULL);
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pPath.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        if (CheckFingerprint(header))
        {
            char* nerv = headerGetNEVR(header, NULL);
            if (nerv != NULL)
            {
                if (CheckHash(header, ts, pPath))
                {
                    ret = nerv;
                    free(nerv);
                }
            }
        }
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    rpmcliFini(context);
    return ret;
}

bool CPackages::Install(const std::string& pPackage)
{
    // TODO: write this
}


bool CPackages::GetInstallationStatus()
{
    GError *error = NULL;
    PkStatusEnum status;
    gboolean ret = pk_client_get_status(m_pPkClient, &status, &error);
    if (ret == FALSE)
    {
        std::string err = error->message;
        g_error_free(error);
        error = NULL;
        throw std::string("CPackages::SearchFile(): ") + err;
    }
    if (status != PK_STATUS_ENUM_FINISHED)
    {
        return false;
    }
    return true;
}




/*
 *
 *
 *
 * std::string CPackages::SearchFile(const std::string& pPath)
{
    std::stringstream ss;
    char *argv[] = {(char*)""};
    poptContext context = rpmcliInit(0, argv, NULL);
    if (context == NULL)
    {
        return "";
    }
    rpmts ts = rpmtsCreate();
    if (ts == NULL)
    {
        rpmcliFini(context);
        return "";
    }
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pPath.c_str(), 0);
    if (iter == NULL)
    {
        rpmtsFree(ts);
        rpmcliFini(context);
        return "";
    }
    Header header;
    char* nerv = NULL;

    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        if (!headerIsEntry(header, RPMTAG_SIGGPG))
        {
            headerFree(header);
            rpmdbFreeIterator(iter);
            rpmtsFree(ts);
            rpmcliFini(context);
            return "";
        }
        char* headerFingerprint;
        rpmtd td = rpmtdNew();
        headerGet(header, RPMTAG_SIGGPG, td, HEADERGET_DEFAULT);
        headerFingerprint =  pgpHexStr((const uint8_t*)td->data + 9, sizeof(pgpKeyId_t));
        rpmtdFree(td);

        if (m_setFingerprints.find(headerFingerprint) == m_setFingerprints.end())
        {
            free(headerFingerprint);
            headerFree(header);
            rpmdbFreeIterator(iter);
            rpmtsFree(ts);
            rpmcliFini(context);
            return "";
        }
        free(headerFingerprint);
        nerv = headerGetNEVR(header, NULL);
        if (nerv == NULL)
        {
            headerFree(header);
            rpmdbFreeIterator(iter);
            rpmcliFini(context);
            rpmtsFree(ts);
            return "";
        }

        td = rpmtdNew();
        rpmfi fi = rpmfiNew(ts, header, RPMTAG_BASENAMES, 0);
        pgpHashAlgo hashAlgo;
        std::string headerHash;
        char computedHash[1024] = "";

        while(rpmfiNext(fi) != -1)
        {
            if (pPath == rpmfiFN(fi))
            {
                headerHash = rpmfiFDigestHex(fi, &hashAlgo);
            }
        }

        rpmDoDigest(hashAlgo, pPath.c_str(), 1, (unsigned char*) computedHash, NULL);

        if (headerHash == "" || std::string(computedHash) == "")
        {
            free(nerv);
            rpmtdFree(td);
            rpmfiFree(fi);
            headerFree(header);
            rpmdbFreeIterator(iter);
            rpmcliFini(context);
            rpmtsFree(ts);
            return "";
        }

        std::string ret = nerv;
        free(nerv);
        rpmtdFree(td);
        rpmfiFree(fi);
        headerFree(header);
        rpmdbFreeIterator(iter);
        rpmcliFini(context);
        rpmtsFree(ts);
        return ret;
    }

    rpmdbFreeIterator(iter);
    rpmcliFini(context);
    rpmtsFree(ts);
    return "";
}
 */

