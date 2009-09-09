/*
    CCpp.cpp

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

#include "abrtlib.h"
#include "CCpp.h"
#include "ABRTException.h"
#include "DebugDump.h"
#include "CommLayerInner.h"
#include <fstream>
#include <sstream>
#include <set>
#include <iomanip>

#include <nss.h>
#include <sechash.h>

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" "DEBUG_DUMPS_DIR" %p %s %u"

#define FILENAME_COREDUMP       "coredump"
#define FILENAME_BACKTRACE      "backtrace"
#define FILENAME_MEMORYMAP      "memorymap"

static pid_t ExecVP(const char* pCommand, char* const pArgs[], uid_t uid, std::string& pOutput);

CAnalyzerCCpp::CAnalyzerCCpp() :
    m_bMemoryMap(false), m_bInstallDebuginfo(true)
{}

static std::string CreateHash(const std::string& pInput)
{
    std::string ret = "";
    HASHContext* hc;
    unsigned char hash[SHA1_LENGTH];
    unsigned int len;

    hc = HASH_Create(HASH_AlgSHA1);
    if (!hc)
    {
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::CreateHash(): cannot initialize hash.");
    }
    HASH_Begin(hc);
    HASH_Update(hc, reinterpret_cast<const unsigned char*>(pInput.c_str()), pInput.length());
    HASH_End(hc, hash, &len, sizeof(hash));
    HASH_Destroy(hc);

    char hash_str[SHA1_LENGTH*2 + 1];
    char *d = hash_str;
    unsigned char *s = hash;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';

    return hash_str;
}

static void InstallDebugInfos(const std::string& pPackage)
{
    update_client(_("Searching for debug-info packages..."));

    std::string packageName = pPackage.substr(0, pPackage.rfind("-", pPackage.rfind("-")-1));
    char buff[1024];
    int pipein[2], pipeout[2];
    pid_t child;

    xpipe(pipein);
    xpipe(pipeout);

    child = fork();
    if (child < 0)
    {
        close(pipein[0]); close(pipeout[0]);
        close(pipein[1]); close(pipeout[1]);
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::InstallDebugInfos():  fork failed.");
    }
    if (child == 0)
    {
        close(pipein[1]);
        close(pipeout[0]);
        xmove_fd(pipein[0], STDIN_FILENO);
        xmove_fd(pipeout[1], STDOUT_FILENO);
        /* Not a good idea, we won't see any error messages */
        /*close(STDERR_FILENO);*/

        setsid();
        execlp("debuginfo-install", "debuginfo-install", "-y", "--", pPackage.c_str(), NULL);
        exit(0);
    }

    close(pipein[0]);
    close(pipeout[1]);

    /* Should not be needed (we use -y option), but just in case: */
    safe_write(pipein[1], "y\n", sizeof("y\n")-1);
    close(pipein[1]);

    update_client(_("Downloading and installing debug-info packages..."));

    FILE *pipeout_fp = fdopen(pipeout[0], "r");
    if (pipeout_fp == NULL) /* never happens */
    {
        close(pipeout[0]);
        wait(NULL);
        return;
    }

/* glx-utils, for example, do not have glx-utils-debuginfo package.
 * Disabled code was causing failures in backtrace decoding.
 * This does not seem to be useful.
 */
#ifdef COMPLAIN_IF_NO_DEBUGINFO
    bool already_installed = false;
#endif
    while (fgets(buff, sizeof(buff), pipeout_fp))
    {
        int last = strlen(buff) - 1;
        if (last >= 0 && buff[last] == '\n')
            buff[last] = '\0';

        /* log(buff); - update_client logs it too */
        update_client(buff); /* maybe only if buff != ""? */

#ifdef COMPLAIN_IF_NO_DEBUGINFO
        if (already_installed == false)
        {
            /* "Package foo-debuginfo-1.2-5.ARCH already installed and latest version" */
            char* pn = strstr(buff, packageName.c_str());
            if (pn)
            {
                char* already_str = strstr(pn, "already installed and latest version");
                if (already_str)
                {
                    already_installed = true;
                }
            }
        }

        if (already_installed == false &&
            (strstr(buff, "No debuginfo packages available to install") != NULL ||
             strstr(buff, "Could not find debuginfo for main pkg") != NULL ||
             strstr(buff, "Could not find debuginfo pkg for dependency package") != NULL))
        {
            fclose(pipeout_fp);
            kill(child, SIGTERM);
            wait(NULL);
            throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": cannot install debuginfos for " + pPackage);
        }
#endif
    }

    fclose(pipeout_fp);
    wait(NULL);
}

static void GetBacktrace(const std::string& pDebugDumpDir, std::string& pBacktrace)
{
    update_client(_("Getting backtrace..."));

    std::string tmpFile = "/tmp/" + pDebugDumpDir.substr(pDebugDumpDir.rfind("/"));
    std::ofstream fTmp;
    std::string UID;
    fTmp.open(tmpFile.c_str());
    if (fTmp.is_open())
    {
        std::string executable;
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_UID, UID);
        fTmp << "file " << executable << std::endl;
        fTmp << "core " << pDebugDumpDir << "/" << FILENAME_COREDUMP << std::endl;
        fTmp << "thread apply all backtrace full" << std::endl;
        fTmp << "q" << std::endl;
        fTmp.close();
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::GetBacktrace(): cannot create gdb script " + tmpFile);
    }
    char* command = (char*)"gdb";
    char* args[5] = { (char*)"gdb", (char*)"-batch", (char*)"-x", NULL, NULL };
    args[3] = (char*) tmpFile.c_str();
    ExecVP(command, args, atoi(UID.c_str()), pBacktrace);
}

static void GetIndependentBacktrace(const std::string& pBacktrace, std::string& pIndependentBacktrace)
{
    int ii = 0;
    std::string line;
    std::string header;
    bool in_bracket = false;
    bool in_quote = false;
    bool in_header = false;
    bool in_digit = false;
    bool has_at = false;
    bool has_filename = false;
    bool has_bracket = false;
    std::set<std::string> set_headers;

    while (ii < pBacktrace.length())
    {
        if (pBacktrace[ii] == '#' && !in_quote)
        {
            if (in_header && !has_filename)
            {
                header = "";
            }
            in_header = true;
        }
        if (in_header)
        {
            if (isdigit(pBacktrace[ii]) && !in_quote && !has_at)
            {
                in_digit = true;
            }
            else if (pBacktrace[ii] == '\\' && pBacktrace[ii + 1] == '\"')
            {
                ii++;
            }
            else if (pBacktrace[ii] == '\"')
            {
                in_quote = in_quote == true ? false : true;
            }
            else if (pBacktrace[ii] == '(' && !in_quote)
            {
                in_bracket = true;
                in_digit = false;
                header += '(';
            }
            else if (pBacktrace[ii] == ')' && !in_quote)
            {
                in_bracket = false;
                has_bracket = true;
                in_digit = false;
                header += ')';
            }
            else if (pBacktrace[ii] == '\n' && has_filename)
            {
                set_headers.insert(header);
                in_bracket = false;
                in_quote = false;
                in_header = false;
                in_digit = false;
                has_at = false;
                has_filename = false;
                has_bracket = false;
                header = "";
            }
            else if (pBacktrace[ii] == ',' && !in_quote)
            {
                in_digit = false;
            }
            else if (isspace(pBacktrace[ii]) && !in_quote)
            {
                in_digit = false;
            }
            else if (pBacktrace[ii] == 'a' && pBacktrace[ii + 1] == 't' && has_bracket && !in_quote)
            {
                has_at = true;
                header += 'a';
            }
            else if (pBacktrace[ii] == ':' && has_at && isdigit(pBacktrace[ii + 1]) && !in_quote)
            {
                has_filename = true;
            }
            else if (in_header && !in_digit && !in_quote && !in_bracket)
            {
                header += pBacktrace[ii];
            }
        }
        ii++;
    }
    pIndependentBacktrace = "";
    std::set<std::string>::iterator it;
    for (it = set_headers.begin(); it != set_headers.end(); it++)
    {
        pIndependentBacktrace += *it;
    }
}

static void GetIndependentBuildIdPC(const std::string& pBuildIdPC, std::string& pIndependentBuildIdPC)
{
    int ii = 0;
    while (ii < pBuildIdPC.length())
    {
        std::string line = "";
        int jj = 0;

        while (pBuildIdPC[ii] != '\n' && ii < pBuildIdPC.length())
        {
            line += pBuildIdPC[ii];
            ii++;
        }
        while (line[jj] != '+' && jj < line.length())
        {
            jj++;
        }
        jj++;
        while (line[jj] != '@' && jj < line.length())
        {
            if (!isspace(line[jj]))
            {
                pIndependentBuildIdPC += line[jj];
            }
            jj++;
        }
        ii++;
    }
}

static pid_t ExecVP(const char* pCommand, char* const pArgs[], uid_t uid, std::string& pOutput)
{
    int pipeout[2];
    char buff[1024];
    pid_t child;

    struct passwd* pw = getpwuid(uid);
    if (!pw)
    {
        throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": cannot get GID for UID.");
    }

    xpipe(pipeout);
    child = fork();
    if (child == -1)
    {
        close(pipeout[0]);
        close(pipeout[1]);
        throw CABRTException(EXCEP_PLUGIN, std::string(__func__) + ": fork failed.");
    }
    if (child == 0)
    {
        close(pipeout[0]); /* read side of the pipe */
        if (pipeout[1] != STDOUT_FILENO)
        {
            dup2(pipeout[1], STDOUT_FILENO);
            close(pipeout[1]);
        }
        /* Make sure stdin is safely open to nothing */
        close(STDIN_FILENO);
        if (open("/dev/null", O_RDONLY))
                if (open("/", O_RDONLY))
                        abort(); /* never happens */
        /* Not a good idea, we won't see any error messages */
        /* close(STDERR_FILENO); */

        setgroups(1, &pw->pw_gid);
        setregid(pw->pw_gid, pw->pw_gid);
        setreuid(uid, uid);
        setsid();

        execvp(pCommand, pArgs);
        exit(0);
    }

    close(pipeout[1]); /* write side of the pipe */

/*
    bool quit = false;

    while (!quit)
    {
        fd_set rsfd;
        FD_ZERO(&rsfd);
        FD_SET(pipeout[0], &rsfd);
        struct timeval delay;

        delay.tv_sec = 1;
        delay.tv_usec = 0;

        if (select(FD_SETSIZE, &rsfd, NULL, NULL, &delay) > 0)
        {
            if (FD_ISSET(pipeout[0], &rsfd))
            {
                int r = read(pipeout[0], buff, sizeof(buff) - 1);
                if (r <= 0)
                {
                    quit = true;
                }
                else
                {
                    buff[r] = '\0';
                    pOutput += buff;
                }
            }
        }
    }
I think the below code has absolutely the same effect:
*/
    int r;
    while ((r = read(pipeout[0], buff, sizeof(buff) - 1)) > 0)
    {
        buff[r] = '\0';
        pOutput += buff;
    }

    close(pipeout[0]);
    wait(NULL); /* why? */

    return 0;
}

std::string CAnalyzerCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{
    update_client(_("Getting local universal unique identification..."));

    std::string UID;
    std::string executable;
    std::string package;
    std::string buildIdPC;
    std::string independentBuildIdPC;
    std::string core = "--core=" + pDebugDumpDir + "/"FILENAME_COREDUMP;
    char* command = (char*)"eu-unstrip";
    char* args[4] = { (char*)"eu-unstrip", NULL, (char*)"-n", NULL };
    args[1] = (char*)core.c_str();

    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_UID, UID);
    dd.LoadText(FILENAME_EXECUTABLE, executable);
    dd.LoadText(FILENAME_PACKAGE, package);
    ExecVP(command, args, atoi(UID.c_str()), buildIdPC);
    dd.Close();

    GetIndependentBuildIdPC(buildIdPC, independentBuildIdPC);
    return CreateHash(package + executable + independentBuildIdPC);
}

std::string CAnalyzerCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    update_client(_("Getting global universal unique identification..."));

    std::string backtrace;
    std::string executable;
    std::string package;
    std::string independentBacktrace;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_BACKTRACE, backtrace);
    dd.LoadText(FILENAME_EXECUTABLE, executable);
    dd.LoadText(FILENAME_PACKAGE, package);
    dd.Close();
    GetIndependentBacktrace(backtrace, independentBacktrace);
    return CreateHash(package + executable + independentBacktrace);
}

void CAnalyzerCCpp::CreateReport(const std::string& pDebugDumpDir)
{
    update_client(_("Starting report creation..."));

    std::string package;
    std::string backtrace;
    CDebugDump dd;

    dd.Open(pDebugDumpDir);
    if (dd.Exist(FILENAME_BACKTRACE))
    {
        return;
    }
    dd.LoadText(FILENAME_PACKAGE, package);
    dd.Close();

    map_plugin_settings_t settings = GetSettings();
    if (settings["InstallDebuginfo"] == "yes")
    {
        InstallDebugInfos(package);
    }
    else
    {
        warn_client(ssprintf(_("Skip debuginfo installation for package %s"), package.c_str()));
    }

    GetBacktrace(pDebugDumpDir, backtrace);

    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_BACKTRACE, backtrace);
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_MEMORYMAP, "memory map of the crashed C/C++ application, not implemented yet");
    }
}

void CAnalyzerCCpp::Init()
{
    std::ifstream fInCorePattern;
    fInCorePattern.open(CORE_PATTERN_IFACE);
    if (fInCorePattern.is_open())
    {
        getline(fInCorePattern, m_sOldCorePattern);
        fInCorePattern.close();
    }
    if (m_sOldCorePattern[0] == '|')
    {
        if (m_sOldCorePattern == CORE_PATTERN)
        {
            log("warning: %s already contains %s, "
                "did abrt daemon crash recently?",
                CORE_PATTERN_IFACE, CORE_PATTERN);
            /* There is no point in "restoring" CORE_PATTERN_IFACE
             * to CORE_PATTERN on exit. Will restore to a default value:
             */
            m_sOldCorePattern = "core";
        } else {
            log("warning: %s was already set to run a crash analyser (%s), "
                "abrt may interfere with it",
                CORE_PATTERN_IFACE, CORE_PATTERN);
        }
    }

    std::ofstream fOutCorePattern;
    fOutCorePattern.open(CORE_PATTERN_IFACE);
    if (fOutCorePattern.is_open())
    {
        fOutCorePattern << CORE_PATTERN << std::endl;
        fOutCorePattern.close();
    }
}

void CAnalyzerCCpp::DeInit()
{
    std::ofstream fOutCorePattern;
    fOutCorePattern.open(CORE_PATTERN_IFACE);
    if (fOutCorePattern.is_open())
    {
        fOutCorePattern << m_sOldCorePattern << std::endl;
        fOutCorePattern.close();
    }
}

void CAnalyzerCCpp::SetSettings(const map_plugin_settings_t& pSettings)
{
    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it = pSettings.find("MemoryMap");
    if (it != end)
    {
        m_bMemoryMap = it->second == "yes";
    }
    it = pSettings.find("DebugInfo");
    if (it != end)
    {
        m_sDebugInfo = it->second;
    }
    it = pSettings.find("InstallDebuginfo");
    if (it != end)
    {
        m_bInstallDebuginfo = it->second == "yes";
    }
}

map_plugin_settings_t CAnalyzerCCpp::GetSettings()
{
    map_plugin_settings_t ret;

    ret["MemoryMap"] = m_bMemoryMap ? "yes" : "no";
    ret["DebugInfo"] = m_sDebugInfo;
    ret["InstallDebuginfo"] = m_bInstallDebuginfo ? "yes" : "no";

    return ret;
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerCCpp,
            "CCpp",
            "0.0.1",
            "Simple C/C++ analyzer plugin.",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
