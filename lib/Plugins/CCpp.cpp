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

#include <sys/wait.h>
#include <fstream>
#include <sstream>
#include <set>
#include <iomanip>
#include <nss.h>
#include <sechash.h>
#include "abrtlib.h"
#include "CCpp.h"
#include "ABRTException.h"
#include "DebugDump.h"
#include "CommLayerInner.h"
#include "Polkit.h"

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" "DEBUG_DUMPS_DIR" %p %s %u"

#define FILENAME_COREDUMP       "coredump"
#define FILENAME_BACKTRACE      "backtrace"
#define FILENAME_MEMORYMAP      "memorymap"

CAnalyzerCCpp::CAnalyzerCCpp() :
    m_bMemoryMap(false), m_bInstallDebuginfo(true)
{}

static bool is_hexstr(const char* str)
{
    while (*str)
    {
        if (!isxdigit(*str))
            return false;
        str++;
    }
    return true;
}

static std::string CreateHash(const std::string& pInput)
{
    std::string ret = "";
    HASHContext* hc;
    unsigned char hash[SHA1_LENGTH];
    unsigned int len;

    hc = HASH_Create(HASH_AlgSHA1);
    if (!hc)
    {
        error_msg_and_die("HASH_Create(HASH_AlgSHA1) failed"); /* paranoia */
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

static pid_t ExecVP(char** pArgs, uid_t uid, std::string& pOutput)
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
        perror_msg_and_die("fork");
    }
    if (child == 0)
    {
        close(pipeout[0]); /* read side of the pipe */
        xmove_fd(pipeout[1], STDOUT_FILENO);
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

        execvp(pArgs[0], pArgs);
        exit(0);
    }

    close(pipeout[1]); /* write side of the pipe */

    int r;
    while ((r = read(pipeout[0], buff, sizeof(buff) - 1)) > 0)
    {
        buff[r] = '\0';
        pOutput += buff;
    }

    close(pipeout[0]);
    wait(NULL); /* prevent having zombie child process */

    return 0;
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
        fTmp << "file " << executable << '\n';
        fTmp << "core " << pDebugDumpDir << "/"FILENAME_COREDUMP"\n";
        fTmp << "thread apply all backtrace full\nq\n";
        fTmp.close();
    }
    else
    {
        throw CABRTException(EXCEP_PLUGIN, "CAnalyzerCCpp::GetBacktrace(): cannot create gdb script " + tmpFile);
    }
    char* args[5];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";
    args[2] = (char*)"-x";
    args[3] = (char*)tmpFile.c_str();
    args[4] = NULL;
    ExecVP(args, atoi(UID.c_str()), pBacktrace);
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

static std::string run_unstrip_n(const std::string& pDebugDumpDir)
{
    std::string UID;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_UID, UID);
    }

    std::string core = "--core=" + pDebugDumpDir + "/"FILENAME_COREDUMP;
    char* args[4];
    args[0] = (char*)"eu-unstrip";
    args[1] = (char*)core.c_str();
    args[2] = (char*)"-n";
    args[3] = NULL;
    std::string output;
    ExecVP(args, atoi(UID.c_str()), output);
    return output;
}

static void InstallDebugInfos(const std::string& pDebugDumpDir)
{
    log("Getting module names, file names, build IDs from core file");
    std::string unstrip_list = run_unstrip_n(pDebugDumpDir);

    log("Builting list of missing debuginfos");
    // lines look like this:
    // 0x400000+0x209000 ab3c8286aac6c043fd1bb1cc2a0b88ec29517d3e@0x40024c /bin/sleep /usr/lib/debug/bin/sleep.debug [exe]
    // 0x7fff313ff000+0x1000 389c7475e3d5401c55953a425a2042ef62c4c7df@0x7fff313ff2f8 . - linux-vdso.so.1
    vector_string_t missing;
    char *dup = xstrdup(unstrip_list.c_str());
    char *p = dup;
    char c;
    do {
        char* end = strchrnul(p, '\n');
        c = *end;
        *end = '\0';
        char* word2 = strchr(p, ' ');
        if (!word2)
            continue;
        word2++;
        char* endsp = strchr(word2, ' ');
        if (!endsp)
            continue;
        /* This filters out linux-vdso.so, among others */
        if (endsp[1] != '/')
            continue;
        *endsp = '\0';
        char* at = strchrnul(word2, '@');
        *at = '\0';

        bool file_exists = 1;
        if (word2[0] && word2[1] && is_hexstr(word2))
        {
            struct stat sb;
            char *fn = xasprintf("/usr/lib/debug/.build-id/%.2s/%s.debug", word2, word2 + 2);
            /* Not lstat: this is a symlink and we want link's TARGET to exist */
            file_exists = stat(fn, &sb) == 0 && S_ISREG(sb.st_mode);
            free(fn);
        }
        log("build_id:%s exists:%d", word2, (int)file_exists);
        if (!file_exists)
            missing.push_back(word2);

        p = end + 1;
    } while (c);
    free(dup);

    if (missing.size() == 0)
    {
        log("All debuginfos are present, not installing debuginfo packages");
        return;
    }
    //missing vector is unused for now, but TODO: use it to install only needed debuginfos

    std::string package;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_PACKAGE, package);
    }

    update_client(_("Searching for debug-info packages..."));

    int pipein[2], pipeout[2];
    xpipe(pipein);
    xpipe(pipeout);

    pid_t child = fork();
    if (child < 0)
    {
        /*close(pipein[0]); close(pipeout[0]); - why bother */
        /*close(pipein[1]); close(pipeout[1]); */
        perror_msg_and_die("fork");
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
        execlp("debuginfo-install", "debuginfo-install", "-y", "--", package.c_str(), NULL);
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
    char buff[1024];
    std::string packageName = package.substr(0, package.rfind("-", package.rfind("-")-1));
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

std::string CAnalyzerCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{
    log(_("Getting local universal unique identification..."));

    std::string executable;
    std::string package;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
    }

    std::string buildIdPC = run_unstrip_n(pDebugDumpDir);
    std::string independentBuildIdPC;
    GetIndependentBuildIdPC(buildIdPC, independentBuildIdPC);
    return CreateHash(package + executable + independentBuildIdPC);
}

std::string CAnalyzerCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    log(_("Getting global universal unique identification..."));

    std::string backtrace;
    std::string executable;
    std::string package;
    std::string independentBacktrace;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_BACKTRACE, backtrace);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
    }
    GetIndependentBacktrace(backtrace, independentBacktrace);
    return CreateHash(package + executable + independentBacktrace);
}

static bool DebuginfoCheckPolkit(int uid)
{
    PolkitResult result;
    int child_pid;

    child_pid = fork();

    if (child_pid == 0)
    {
         //child
        setuid(uid);
        result = polkit_check_authorization(getpid(),
                 "org.fedoraproject.abrt.install-debuginfos");
        if (result == PolkitYes)
        {
            exit(0); //authentication OK
        }
        exit(1);
    } else
    {
        //parent
        int status;

        waitpid(child_pid, &status, 0);
        if (WEXITSTATUS(status) == 0)
        {
            return true; //authentication OK
        }
        return false;
    }

}

void CAnalyzerCCpp::CreateReport(const std::string& pDebugDumpDir)
{
    update_client(_("Starting report creation..."));

    std::string package;
    std::string backtrace;
    std::string UID;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    bool bt_exists = dd.Exist(FILENAME_BACKTRACE);

    if (bt_exists)
    {
        dd.Close(); /* do not keep dir locked longer than needed */
        return; /* already done */
    }

    dd.LoadText(FILENAME_PACKAGE, package);
    dd.LoadText(FILENAME_UID, UID);
    dd.Close();


    map_plugin_settings_t settings = GetSettings();
    if (settings["InstallDebuginfo"] == "yes" &&
        DebuginfoCheckPolkit(atoi(UID.c_str())) )
    {
        InstallDebugInfos(pDebugDumpDir);
    }
    else
    {
        warn_client(_("Skipping debuginfo installation"));
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
