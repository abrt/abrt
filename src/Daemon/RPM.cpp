/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "abrtlib.h"
#include "RPM.h"
#include "comm_layer_inner.h"

CRPM::CRPM()
{
    static const char *const argv[] = { "", NULL };
    m_poptContext = rpmcliInit(1, (char**)argv, NULL);
}

CRPM::~CRPM()
{
    rpmFreeCrypto();
    rpmFreeRpmrc();
    rpmcliFini(m_poptContext);
}

void CRPM::LoadOpenGPGPublicKey(const char* pFileName)
{
    uint8_t* pkt = NULL;
    size_t pklen;
    pgpKeyID_t keyID;
    if (pgpReadPkts(pFileName, &pkt, &pklen) != PGPARMOR_PUBKEY)
    {
        free(pkt);
        error_msg("Can't load public GPG key %s", pFileName);
        return;
    }
    if (pgpPubkeyFingerprint(pkt, pklen, keyID) == 0)
    {
        char* fedoraFingerprint = pgpHexStr(keyID, sizeof(keyID));
        if (fedoraFingerprint != NULL)
        {
            m_setFingerprints.insert(fedoraFingerprint);
            free(fedoraFingerprint);
        }
    }
    free(pkt);
}

bool CRPM::CheckFingerprint(const char* pPackage)
{
    bool ret = false;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);

    if (header != NULL)
    {
        rpmTag rpmTags[] = { RPMTAG_DSAHEADER, RPMTAG_RSAHEADER, RPMTAG_SHA1HEADER };
        int ii;
        for (ii = 0; ii < 3; ii++)
        {
            if (headerIsEntry(header, rpmTags[ii]))
            {
                rpmtd td = rpmtdNew();
                headerGet(header, rpmTags[ii] , td, HEADERGET_DEFAULT);
                char* pgpsig = rpmtdFormat(td, RPMTD_FORMAT_PGPSIG , NULL);
                rpmtdFree(td);
                if (pgpsig)
                {
                    std::string PGPSignatureText = pgpsig;
                    free(pgpsig);

                    size_t Key_ID_pos = PGPSignatureText.find(" Key ID ");
                    if (Key_ID_pos != std::string::npos)
                    {
                        std::string headerFingerprint = PGPSignatureText.substr(Key_ID_pos + sizeof (" Key ID ") - 1);

                        if (headerFingerprint != "")
                        {
                            if (m_setFingerprints.find(headerFingerprint) != m_setFingerprints.end())
                            {
                                ret = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

bool CheckHash(const char* pPackage, const char* pPath)
{
    bool ret = false;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);
    if (header != NULL)
    {
        rpmfi fi = rpmfiNew(ts, header, RPMTAG_BASENAMES, RPMFI_NOHEADER);
        std::string headerHash;
        char computedHash[1024] = "";

        while (rpmfiNext(fi) != -1)
        {
            if (strcmp(pPath, rpmfiFN(fi)) == 0)
            {
                headerHash = rpmfiFDigestHex(fi, NULL);
                rpmDoDigest(rpmfiDigestAlgo(fi), pPath, 1, (unsigned char*) computedHash, NULL);
                ret = (headerHash != "" && headerHash == computedHash);
                break;
            }
        }
        rpmfiFree(fi);
    }
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

std::string GetDescription(const char* pPackage)
{
    std::string pDescription;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);
    if (header != NULL)
    {
        rpmtd td = rpmtdNew();
        headerGet(header, RPMTAG_SUMMARY, td, HEADERGET_DEFAULT);
        const char* summary = rpmtdGetString(td);
        headerGet(header, RPMTAG_DESCRIPTION, td, HEADERGET_DEFAULT);
        const char* description = rpmtdGetString(td);
        pDescription = summary + std::string("\n\n") + description;
        rpmtdFree(td);
    }
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return pDescription;
}

std::string GetComponent(const char* pFileName)
{
    std::string ret;
    char *package_name;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pFileName, 0);
    Header header = rpmdbNextIterator(iter);
    if (header != NULL)
    {
        rpmtd td = rpmtdNew();
        headerGet(header, RPMTAG_SOURCERPM, td, HEADERGET_DEFAULT);
        const char * srpm = rpmtdGetString(td);
        if (srpm != NULL)
        {
            package_name = get_package_name_from_NVR_or_NULL(srpm);
            ret = std::string(package_name);
            free(package_name);
        }
        rpmtdFree(td);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

char* GetPackage(const char* pFileName)
{
    char* ret = NULL;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pFileName, 0);
    Header header = rpmdbNextIterator(iter);
    if (header != NULL)
    {
        ret = headerGetNEVR(header, NULL);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}
