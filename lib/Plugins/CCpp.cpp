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

#include "CCpp.h"
#include "ABRTException.h"
#include "DebugDump.h"
#include "PluginSettings.h"
#include "CommLayerInner.h"
#include <fstream>
#include <sstream>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <iomanip>
#include <grp.h>
#include <pwd.h>

#include <nss.h>
#include <sechash.h>

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" "DEBUG_DUMPS_DIR" %p %s %u"

#define FILENAME_COREDUMP       "coredump"
#define FILENAME_BACKTRACE      "backtrace"
#define FILENAME_MEMORYMAP      "memorymap"

CAnalyzerCCpp::CAnalyzerCCpp() :
        m_bMemoryMap(false),
        m_Pid(0)
{}

CAnalyzerCCpp::~CAnalyzerCCpp()
{
    if (m_Pid)
    {
        kill(m_Pid, SIGTERM);
        wait(NULL);
    }
}

std::string CAnalyzerCCpp::CreateHash(const std::string& pInput)
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

    unsigned int ii;
    std::stringstream ss;
    for (ii = 0; ii < len; ii++)
        ss <<  std::setw(2) << std::setfill('0') << std::hex << (hash[ii]&0xff);

    return ss.str();
}

void CAnalyzerCCpp::InstallDebugInfos(const std::string& pPackage)
{
    comm_layer_inner_status("Searching for debug-info packages...");

    std::string packageName = pPackage.substr(0, pPackage.rfind("-", pPackage.rfind("-")-1));
    char buff[1024];
    int pipein[2], pipeout[2];
    struct timeval delay;
    pid_t child;

    pipe(pipein); /* error check? */
    pipe(pipeout);

    child = fork();
    m_Pid = child;
    if (child < 0)
    {
        close(pipein[0]); close(pipeout[0]);
        close(pipein[1]); close(pipeout[1]);
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::InstallDebugInfos():  fork failed.");
    }
    if (child == 0)
    {
        if (pipein[0] != STDIN_FILENO)
        {
            dup2(pipein[0], STDIN_FILENO);
            close(pipein[0]);
        }
        if (pipeout[1] != STDOUT_FILENO)
        {
            dup2(pipeout[1], STDOUT_FILENO);
            close(pipeout[1]);
        }
        /* Not a good idea, we won't see any error messages */
        /*close(STDERR_FILENO);*/

        setsid();
        execlp("debuginfo-install", "debuginfo-install", pPackage.c_str(), NULL);
        exit(0);
    }

    close(pipein[0]);
    close(pipeout[1]);

    bool already_installed = false;

    while (1)
    {
/* Does not seem to be needed
        fd_set rsfd;
        FD_ZERO(&rsfd);

        FD_SET(pipeout[0], &rsfd);

        delay.tv_sec = 1;
        delay.tv_usec = 0;

        if (select(FD_SETSIZE, &rsfd, NULL, NULL, &delay) <= 0)
            continue;
        if (!FD_ISSET(pipeout[0], &rsfd))
            continue;
*/
        int r = read(pipeout[0], buff, sizeof(buff) - 1);
        if (r <= 0)
            break;

        buff[r] = '\0';
        comm_layer_inner_debug(buff);
        if (strstr(buff, packageName.c_str()) != NULL &&
            strstr(buff, "already installed and latest version") != NULL)
        {
            char* ii = strstr(buff, packageName.c_str());
            char* jj = strstr(ii, "\n");
            char* kk = strstr(ii, "already installed and latest version");

            if (jj > kk)
            {
                already_installed = true;
            }
        }
        if (already_installed == false &&
            (strstr(buff, "No debuginfo packages available to install") != NULL ||
             strstr(buff, "Could not find debuginfo for main pkg") != NULL ||
             strstr(buff, "Could not find debuginfo pkg for dependency package") != NULL))
        {
            close(pipein[1]);
            close(pipeout[0]);
            kill(child, SIGTERM);
            wait(NULL);
            throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage);
        }
        if (strstr(buff, "Total download size") != NULL)
        {
            int r = write(pipein[1], "y\n", sizeof("y\n"));
            if (r != sizeof("y\n"))
            {
                close(pipein[1]);
                close(pipeout[0]);
                kill(child, SIGTERM);
                wait(NULL);
                throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::InstallDebugInfos(): cannot install debuginfos for " + pPackage);
            }
            comm_layer_inner_status("Downloading and installing debug-info packages...");
        }
    }
    close(pipein[1]);
    close(pipeout[0]);

    wait(NULL);
    m_Pid = 0;
}

void CAnalyzerCCpp::GetBacktrace(const std::string& pDebugDumpDir, std::string& pBacktrace)
{
    comm_layer_inner_status("Getting backtrace...");

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
        dd.Close();
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
    args[3] = strdup(tmpFile.c_str());
    ExecVP(command, args, UID, pBacktrace);
    free(args[3]);
}

void CAnalyzerCCpp::GetIndependentBacktrace(const std::string& pBacktrace, std::string& pIndependentBacktrace)
{
    int ii = 0;
    std::string line;
    std::string header;
    int jj = 0;
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

void CAnalyzerCCpp::GetIndependentBuildIdPC(const std::string& pBuildIdPC, std::string& pIndependentBuildIdPC)
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

gid_t CAnalyzerCCpp::GetGIDFromUID(const std::string& pUID)
{
    struct passwd* pw;

    while (( pw = getpwent()) != NULL)
    {
        if (pw->pw_uid == atoi(pUID.c_str()))
        {
            setpwent();
            return pw->pw_gid;
        }
    }
    setpwent();
    return -1;
}

void CAnalyzerCCpp::ExecVP(const char* pCommand, char* const pArgs[], const std::string& pUID, std::string& pOutput)
{
    int pipeout[2];
    char buff[1024];
    struct timeval delay;
    pid_t child;
    gid_t GID[1];

    if ((GID[0] = GetGIDFromUID(pUID)) == -1)
    {
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::ExecVP(): cannot get GUI for UID.");
    }

    pipe(pipeout);  /* error check? */
    child = fork();
    m_Pid = child;
    if (child == -1)
    {
        close(pipeout[0]);
        close(pipeout[1]);
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::ExecVP(): fork failed.");
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

        setgroups(1, GID);
        setregid(atoi(pUID.c_str()), atoi(pUID.c_str()));
        setreuid(atoi(pUID.c_str()), atoi(pUID.c_str()));
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
    m_Pid = 0;
}

std::string CAnalyzerCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{
    comm_layer_inner_status("Getting local universal unique identification...");

    CDebugDump dd;
    std::string UID;
    std::string executable;
    std::string package;
    std::string buildIdPC;
    std::string independentBuildIdPC;
    std::string core = "--core="+ pDebugDumpDir + "/" +FILENAME_COREDUMP;
    char* command = (char*)"eu-unstrip";
    char* args[4] = { (char*)"eu-unstrip", NULL, (char*)"-n", NULL };
    args[1] = strdup(core.c_str());
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_UID, UID);
    dd.LoadText(FILENAME_EXECUTABLE, executable);
    dd.LoadText(FILENAME_PACKAGE, package);
    ExecVP(command, args, UID, buildIdPC);
    dd.Close();
    free(args[1]);
    GetIndependentBuildIdPC(buildIdPC, independentBuildIdPC);
    return CreateHash(package + executable + independentBuildIdPC);
}
std::string CAnalyzerCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    comm_layer_inner_status("Getting global universal unique identification...");

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
    comm_layer_inner_status("Starting report creation...");

    std::string package;
    std::string backtrace;
    CDebugDump dd;

    dd.Open(pDebugDumpDir);
    if (dd.Exist(FILENAME_BACKTRACE))
    {
        dd.Close();
        return;
    }
    dd.LoadText(FILENAME_PACKAGE, package);
    dd.Close();

    InstallDebugInfos(package);

    GetBacktrace(pDebugDumpDir, backtrace);

    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_BACKTRACE, backtrace);
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_MEMORYMAP, "memory map of the crashed C/C++ application, not implemented yet");
    }
    dd.Close();
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

void CAnalyzerCCpp::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    plugin_load_settings(pPath, settings);

    if (settings.find("MemoryMap") != settings.end())
    {
        m_bMemoryMap = settings["MemoryMap"] == "yes";
    }
}
