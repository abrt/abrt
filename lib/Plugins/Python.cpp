#include "Pyhon.h"
#include "DebugDump.h"
#include <sstream>

#include <nss.h>
#include <sechash.h>
#include <prinit.h>

#define FILENAME_BACKTRACE      "backtrace"

std::string CAnalyzerPython::CreateHash(const std::string& pInput)
{
    std::string ret = "";
    HASHContext* hc;
    unsigned char hash[SHA1_LENGTH];
    unsigned int len;

    hc = HASH_Create(HASH_AlgSHA1);
    if (!hc)
    {
        throw std::string("CAnalyzerPython::CreateHash(): cannot initialize hash.");
    }
    HASH_Begin(hc);
    HASH_Update(hc, reinterpret_cast<const unsigned char*>(pInput.c_str()), pInput.length());
    HASH_End(hc, hash, &len, sizeof(hash));
    HASH_Destroy(hc);

    unsigned int ii;
    std::stringstream ss;
    for (ii = 0; ii < len; ii++)
        ss <<  std::setw(2) << std::setfill('0') << std::hex << (hash[ii]&0xff);

    return ss.str();
}

std::string CAnalyzerPython::GetLocalUUID(const std::string& pDebugDumpDir)
{
    CDebugDump dd;
    std::string executable;
    std::string package;
    std::string backtrace;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_EXECUTABLE, executable);
    dd.LoadText(FILENAME_PACKAGE, package);
    dd.LoadText(FILENAME_BACKTRACE, backtrace);
    dd.Close();

    // TODO: get independent backtrace

    return CreateHash(package + executable + backtrace );
}
std::string CAnalyzerPython::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerPython::Init()
{
    // TODO: Copy abrt exception handler to proper place
    if (NSS_NoDB_Init(NULL) != SECSuccess)
    {
        throw std::string("CAnalyzerPython::Init(): cannot initialize NSS library.");
    }
}


void CAnalyzerPython::DeInit()
{
    // TODO: remove copied abrt exception handler
    NSS_Shutdown();
}
