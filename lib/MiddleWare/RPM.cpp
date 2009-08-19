#include "RPM.h"
#include "CommLayerInner.h"

CRPM::CRPM()
{
    char *argv[] = {(char*)""};
    m_poptContext = rpmcliInit(0, argv, NULL);
}

CRPM::~CRPM()
{
    rpmcliFini(m_poptContext);
}

void CRPM::LoadOpenGPGPublicKey(const std::string& pFileName)
{
    uint8_t* pkt = NULL;
    size_t pklen;
    pgpKeyID_t keyID;
    if (pgpReadPkts(pFileName.c_str(), &pkt, &pklen) != PGPARMOR_PUBKEY)
    {
        free(pkt);
        comm_layer_inner_warning("CRPM::LoadOpenGPGPublicKey(): Can not load public key " + pFileName);
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

bool CRPM::CheckFingerprint(const std::string& pPackage)
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

bool CRPM::CheckHash(const std::string& pPackage, const std::string& pPath)
{
    bool ret = false;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        rpmfi fi = rpmfiNew(ts, header, RPMTAG_BASENAMES, RPMFI_NOHEADER);
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

std::string CRPM::GetDescription(const std::string& pPackage)
{
    std::string pDescription = "";
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
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
std::string CRPM::GetComponent(const std::string& pFileName)
{
    std::string ret = "";
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, pFileName.c_str(), 0);
    Header header;
    if ((header = rpmdbNextIterator(iter)) != NULL)
    {
        rpmtd td = rpmtdNew();
        headerGet(header,RPMTAG_SOURCERPM, td, HEADERGET_DEFAULT);
        const char * srpm = rpmtdGetString(td);
        if (srpm != NULL)
        {
            std::string srcrpm(srpm);
            ret = srcrpm.erase(srcrpm.rfind('-',srcrpm.rfind('-')-1));
        }
        rpmtdFree(td);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}
std::string CRPM::GetPackage(const std::string& pFileName)
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
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}
