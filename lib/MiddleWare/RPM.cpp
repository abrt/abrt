#include "RPMInfo.h"
#include <iostream>

CRPMInfo::CRPMInfo()
{
    char *argv[] = {(char*)""};
    m_poptContext = rpmcliInit(0, argv, NULL);
}

CRPMInfo::~CRPMInfo()
{
    rpmcliFini(m_poptContext);
}

void CRPMInfo::LoadOpenGPGPublicKey(const std::string& pFileName)
{
    uint8_t* pkt = NULL;
    size_t pklen;
    pgpKeyID_t keyID;
    if (pgpReadPkts(pFileName.c_str(), &pkt, &pklen) != PGPARMOR_PUBKEY)
    {
        free(pkt);
        std::cerr << "CRPMInfo::LoadOpenGPGPublicKey(): Can not load public key " + pFileName << std::endl;
        return;
    }
    if (pgpPubkeyFingerprint(pkt, pklen, keyID) == 0)
    {
        char* fedoraFingerprint = pgpHexStr(keyID, sizeof(keyID));
        if (fedoraFingerprint != NULL)
        {
            m_setFingerprints.insert(fedoraFingerprint);
        }
    }
    free(pkt);
}

bool CRPMInfo::CheckFingerprint(const std::string& pPackage)
{
    bool ret = false;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        if (headerIsEntry(header, RPMTAG_SIGGPG))
        {
            char* headerFingerprint;
            rpmtd td = rpmtdNew();
            headerGet(header, RPMTAG_SIGGPG, td, HEADERGET_DEFAULT);
            headerFingerprint = pgpHexStr((const uint8_t*)td->data + 9, sizeof(pgpKeyID_t));
            rpmtdFree(td);
            if (headerFingerprint != NULL)
            {
                if (m_setFingerprints.find(headerFingerprint) != m_setFingerprints.end())
                {
                    free(headerFingerprint);
                    ret = true;
                }
            }
        }
    }
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

bool CRPMInfo::CheckHash(const std::string& pPackage, const std::string& pPath)
{
    bool ret = false;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
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
        rpmfiFree(fi);

        rpmDoDigest(hashAlgo, pPath.c_str(), 1, (unsigned char*) computedHash, NULL);

        if (headerHash != "" && headerHash == computedHash)
        {
            ret = true;
        }
    }
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

std::string CRPMInfo::GetPackage(const std::string& pFileName, std::string& pDescription)
{
    std::string ret = "";
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pFileName.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        char* nerv = headerGetNEVR(header, NULL);
        if (nerv != NULL)
        {
            ret = nerv;
            free(nerv);
        }
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
    return ret;
}

