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

static std::string concat_str_vector(char **strings)
{
    std::string result;
    while (*strings)
    {
        result += *strings++;
        if (*strings)
            result += ' ';
    }
    return result;
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
        VERB1 log("Executing: %s", concat_str_vector(pArgs).c_str());
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
        /* VERB1 since sometimes we expect errors here */
        VERB1 perror_msg("Can't execute '%s'", pArgs[0]);
        exit(1);
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

    std::string UID;
    std::string executable;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_UID, UID);
    }

    char* args[9];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";
    // when/if gdb supports it:
    // (https://bugzilla.redhat.com/show_bug.cgi?id=528668):
    //args[2] = (char*)"-ex";
    //args[3] = "set debug-file-directory /usr/lib/debug/.build-id:/var/cache/abrt-di/usr/lib/debug/.build-id";
    /*
     * Unfortunately, "file BINARY_FILE" doesn't work well if BINARY_FILE
     * was deleted (as often happens during system updates):
     * gdb uses specified BINARY_FILE
     * even if it is completely unrelated to the coredump
     * See https://bugzilla.redhat.com/show_bug.cgi?id=525721
     */
    args[2] = (char*)"-ex";
    args[3] = xasprintf("file %s", executable.c_str());
    args[4] = (char*)"-ex";
    args[5] = xasprintf("core-file %s/"FILENAME_COREDUMP, pDebugDumpDir.c_str());
    args[6] = (char*)"-ex";
    args[7] = (char*)"thread apply all backtrace full";
    args[8] = NULL;

    ExecVP(args, atoi(UID.c_str()), pBacktrace);

    free(args[3]);
    free(args[5]);
}

static std::string GetIndependentBacktrace(const std::string& pBacktrace)
{
    std::string header;
    bool in_bracket = false;
    bool in_quote = false;
    bool in_header = false;
    bool in_digit = false;
    bool has_at = false;
    bool has_filename = false;
    bool has_bracket = false;
    std::set<std::string> set_headers;

    /* Backtrace example:
    #0  0x00007f047e21af70 in __nanosleep_nocancel () from /lib64/libc-2.10.1.so

    Thread 1 (Thread 30750):
    #0  0x00007f047e21af70 in __nanosleep_nocancel () from /lib64/libc-2.10.1.so
    No symbol table info available.
    #1  0x00000000004037bb in rpl_nanosleep (requested_delay=0x7fff8999e400,
        remaining_delay=0x0) at nanosleep.c:69
            r = -516
            delay = {tv_sec = 1260, tv_nsec = 0}
            t0 = {tv_sec = 12407, tv_nsec = 291505364}
    #2  0x000000000040322b in xnanosleep (seconds=<value optimized out>)
        at xnanosleep.c:112
            overflow = false
            ts_sleep = {tv_sec = 1260, tv_nsec = 0}
            __PRETTY_FUNCTION__ = "xnanosleep"
    #3  0x0000000000401779 in main (argc=2, argv=0x7fff8999e598) at sleep.c:147
            i = 2
            seconds = 1260
            ok = true
    */
    const char *bk = pBacktrace.c_str();
    while (*bk)
    {
        if (bk[0] == '#'
         && bk[1] >= '0' && bk[1] <= '7'
         && bk[2] == ' ' /* take only #0...#7 (8 last stack frames) */
         && !in_quote
        ) {
            if (in_header && !has_filename)
            {
                header = "";
            }
            in_header = true;
        }
        if (in_header)
        {
            if (isdigit(*bk) && !in_quote && !has_at)
            {
                in_digit = true;
            }
            else if (bk[0] == '\\' && bk[1] == '\"')
            {
                bk++;
            }
            else if (*bk == '\"')
            {
                in_quote = in_quote == true ? false : true;
            }
            else if (*bk == '(' && !in_quote)
            {
                in_bracket = true;
                in_digit = false;
                header += '(';
            }
            else if (*bk == ')' && !in_quote)
            {
                in_bracket = false;
                has_bracket = true;
                in_digit = false;
                header += ')';
            }
            else if (*bk == '\n' && has_filename)
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
            else if (*bk == ',' && !in_quote)
            {
                in_digit = false;
            }
            else if (isspace(*bk) && !in_quote)
            {
                in_digit = false;
            }
            else if (bk[0] == 'a' && bk[1] == 't' && has_bracket && !in_quote)
            {
                has_at = true;
                header += 'a';
            }
            else if (bk[0] == ':' && has_at && isdigit(bk[1]) && !in_quote)
            {
                has_filename = true;
            }
            else if (in_header && !in_digit && !in_quote && !in_bracket)
            {
                header += *bk;
            }
        }
        bk++;
    }

    std::string pIndependentBacktrace;
    std::set<std::string>::iterator it = set_headers.begin();
    for (; it != set_headers.end(); it++)
    {
        pIndependentBacktrace += *it;
    }
    VERB3 log("IndependentBacktrace:'%s'", pIndependentBacktrace.c_str());
    return pIndependentBacktrace;
}

static void GetIndependentBuildIdPC(const std::string& pBuildIdPC, std::string& pIndependentBuildIdPC)
{
    int ii = 0;
    while (ii < pBuildIdPC.length())
    {
        std::string line;
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

    char* args[4];
    args[0] = (char*)"eu-unstrip";
    args[1] = xasprintf("--core=%s/"FILENAME_COREDUMP, pDebugDumpDir.c_str());
    args[2] = (char*)"-n";
    args[3] = NULL;

    std::string output;
    ExecVP(args, atoi(UID.c_str()), output);

    free(args[1]);

    return output;
}

static void InstallDebugInfos(const std::string& pDebugDumpDir, std::string& build_ids)
{
    update_client(_("Searching for debug-info packages..."));

    int pipein[2], pipeout[2]; //TODO: get rid of pipein
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

        char *coredump = xasprintf("%s/"FILENAME_COREDUMP, pDebugDumpDir.c_str());
        char *tempdir = xasprintf("/tmp/abrt-%u-%lu", (int)getpid(), (long)time(NULL));
        /* log() goes to stderr/syslog, it's ok to use it here */
        VERB1 log("Executing: %s %s %s %s", "abrt-debuginfo-install", coredump, tempdir, "/"); // "/var/cache/abrt-di"
        execlp("abrt-debuginfo-install", "abrt-debuginfo-install", coredump, tempdir, "/", NULL);
        exit(1);
    }

    close(pipein[0]);
    close(pipeout[1]);

    update_client(_("Downloading and installing debug-info packages..."));

    FILE *pipeout_fp = fdopen(pipeout[0], "r");
    if (pipeout_fp == NULL) /* never happens */
    {
        close(pipeout[0]);
        wait(NULL);
        return;
    }

    char buff[1024];
    while (fgets(buff, sizeof(buff), pipeout_fp))
    {
        int last = strlen(buff) - 1;
        if (last >= 0 && buff[last] == '\n')
            buff[last] = '\0';

        if (strncmp(buff, "MISSING:", 8) == 0)
        {
            build_ids += "Debuginfo absent: ";
            build_ids += buff + 8;
            build_ids += "\n";
        }

        const char *p = buff;
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        if (*p)
        {
            /* log(buff); - update_client logs it too */
            update_client(buff);
        }
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
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_BACKTRACE, backtrace);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
    }
    std::string independentBacktrace = GetIndependentBacktrace(backtrace);
    return CreateHash(package + executable + independentBacktrace);
}

static bool DebuginfoCheckPolkit(int uid)
{
    int child_pid = fork();
    if (child_pid < 0)
    {
        perror_msg_and_die("fork");
    }
    if (child_pid == 0)
    {
        //child
        if (setuid(uid))
            exit(1); //paranoia
        PolkitResult result = polkit_check_authorization(getpid(),
                 "org.fedoraproject.abrt.install-debuginfos");
        exit(result != PolkitYes); //exit 1 (failure) if not allowed
    }

    //parent
    int status;
    if (waitpid(child_pid, &status, 0) > 0 && WEXITSTATUS(status) == 0)
    {
        return true; //authorization OK
    }
    log("UID %d is not authorized to install debuginfos", uid);
    return false;
}

void CAnalyzerCCpp::CreateReport(const std::string& pDebugDumpDir, int force)
{
    update_client(_("Starting report creation..."));

    std::string package;
    std::string backtrace;
    std::string UID;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    if (!force)
    {
        bool bt_exists = dd.Exist(FILENAME_BACKTRACE);
        if (bt_exists)
        {
            return; /* backtrace already exists */
        }
    }

    dd.LoadText(FILENAME_PACKAGE, package);
    dd.LoadText(FILENAME_UID, UID);
    dd.Close(); /* do not keep dir locked longer than needed */

    std::string build_ids;
    map_plugin_settings_t settings = GetSettings();
    if (settings["InstallDebuginfo"] == "yes" &&
        DebuginfoCheckPolkit(atoi(UID.c_str())) )
    {
        InstallDebugInfos(pDebugDumpDir, build_ids);
    }
    else
    {
        VERB1 log(_("Skipping debuginfo installation"));
    }

    GetBacktrace(pDebugDumpDir, backtrace);

    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_BACKTRACE, build_ids + backtrace);
log("BACKTRACE:'%s'", (build_ids + backtrace).c_str());
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
