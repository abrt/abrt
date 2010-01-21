/*
    Firefox.cpp

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
#include "Firefox.h"
#include "ABRTException.h"
#include "DebugDump.h"
#include "CommLayerInner.h"
#include "Polkit.h"

#define CORE_PATTERN_IFACE      "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN            "|"CCPP_HOOK_PATH" "DEBUG_DUMPS_DIR" %p %s %u"

#define DEBUGINFO_CACHE_DIR     LOCALSTATEDIR"/cache/abrt-di"

CAnalyzerFirefox::CAnalyzerFirefox() :
    m_bBacktrace(true),
    m_bMemoryMap(false),
    m_bInstallDebugInfo(true),
    m_nDebugInfoCacheMB(4000)
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
    pid_t child;

    xpipe(pipeout);

    fflush(NULL);
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
        xmove_fd(xopen("/dev/null", O_RDONLY), STDIN_FILENO);
        /* Not a good idea, we won't see any error messages */
        /* close(STDERR_FILENO); */

        struct passwd* pw = getpwuid(uid);
        gid_t gid = pw ? pw->pw_gid : uid;
        setgroups(1, &gid);
        xsetregid(gid, gid);
        xsetreuid(uid, uid);

        /* Nuke everything which may make setlocale() switch to non-POSIX locale:
         * we need to avoid having gdb output in some obscure language.
         */
        unsetenv("LANG");
        unsetenv("LC_ALL");
        unsetenv("LC_COLLATE");
        unsetenv("LC_CTYPE");
        unsetenv("LC_MESSAGES");
        unsetenv("LC_MONETARY");
        unsetenv("LC_NUMERIC");
        unsetenv("LC_TIME");

        execvp(pArgs[0], pArgs);
        /* VERB1 since sometimes we expect errors here */
        VERB1 perror_msg("Can't execute '%s'", pArgs[0]);
        exit(1);
    }

    close(pipeout[1]); /* write side of the pipe */

    int r;
    char buff[1024];
    while ((r = read(pipeout[0], buff, sizeof(buff) - 1)) > 0)
    {
        buff[r] = '\0';
        pOutput += buff;
    }

    close(pipeout[0]);
    waitpid(child, NULL, 0); /* prevent having zombie child process */

    return 0;
}

enum LineRating
{
    // RATING              EXAMPLE
    MissingEverything = 0, // #0 0x0000dead in ?? ()
    MissingFunction   = 1, // #0 0x0000dead in ?? () from /usr/lib/libfoobar.so.4
    MissingLibrary    = 2, // #0 0x0000dead in foobar()
    MissingSourceFile = 3, // #0 0x0000dead in FooBar::FooBar () from /usr/lib/libfoobar.so.4
    Good              = 4, // #0 0x0000dead in FooBar::crash (this=0x0) at /home/user/foobar.cpp:204
    BestRating = Good,
};

static LineRating rate_line(const char *line)
{
#define FOUND(x) (strstr(line, x) != NULL)
    /* see the "enum LineRating" comments for possible combinations */
    if (FOUND(" at "))
        return Good;
    const char *function = strstr(line, " in ");
    if (function)
    {
        if (function[4] == '?') /* " in ??" does not count */
        {
            function = NULL;
        }
    }
    bool library = FOUND(" from ");
    if (function && library)
        return MissingSourceFile;
    if (function)
        return MissingLibrary;
    if (library)
        return MissingFunction;

    return MissingEverything;
#undef FOUND
}

/* returns number of "stars" to show */
static int rate_backtrace(const char *backtrace)
{
    int i, j, len;
    int multiplier = 0;
    int rating = 0;
    int best_possible_rating = 0;

    /* We look at the frames in reversed order, since:
     * - rate_line() checks starting from the first line of the frame
     * (note: it may need to look at more than one line!)
     * - we increase weight (multiplier) for every frame,
     *   so that topmost frames end up most important
     */
    len = 0;
    for (i = strlen(backtrace) - 1; i >= 0; i--)
    {
        if (backtrace[i] == '#') /* this separates frames from each other */
        {
            std::string s(backtrace + i + 1, len);
            for (j=0; j<len; j++) /* replace tabs with spaces */
                if (s[j] == '\t')
                    s[j] = ' ';
            multiplier++;
            rating += rate_line(s.c_str()) * multiplier;
            best_possible_rating += BestRating * multiplier;
            len = 0; /* starting new line */
        }
        else
        {
            len++;
        }
    }

    /* Bogus 'backtrace' with zero frames? */
    if (best_possible_rating == 0)
        return 0;

    /* Returning number of "stars" to show */
    if (rating*10 >= best_possible_rating*8) /* >= 0.8 */
        return 4;
    if (rating*10 >= best_possible_rating*6)
        return 3;
    if (rating*10 >= best_possible_rating*4)
        return 2;
    if (rating*10 >= best_possible_rating*2)
        return 1;

    return 0;
}

static void GetBacktrace(const char *pDebugDumpDir, std::string& pBacktrace)
{
    update_client(_("Generating backtrace"));

    std::string UID;
    std::string executable;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_UID, UID);
    }

    // Workaround for
    // http://sourceware.org/bugzilla/show_bug.cgi?id=9622
    unsetenv("TERM");
    putenv((char*)"TERM=dumb");

    char* args[11];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";
    // when/if gdb supports it:
    // (https://bugzilla.redhat.com/show_bug.cgi?id=528668):
    args[2] = (char*)"-ex";
    args[3] = (char*)"set debug-file-directory /usr/lib/debug:" DEBUGINFO_CACHE_DIR"/usr/lib/debug";
    /*
     * Unfortunately, "file BINARY_FILE" doesn't work well if BINARY_FILE
     * was deleted (as often happens during system updates):
     * gdb uses specified BINARY_FILE
     * even if it is completely unrelated to the coredump
     * See https://bugzilla.redhat.com/show_bug.cgi?id=525721
     */
    args[4] = (char*)"-ex";
    args[5] = xasprintf("file %s", executable.c_str());
    args[6] = (char*)"-ex";
    args[7] = xasprintf("core-file %s/"FILENAME_COREDUMP, pDebugDumpDir);
    args[8] = (char*)"-ex";
    /* max 3000 frames: with no limit, gdb sometimes OOMs the machine */
    args[9] = (char*)"thread apply all backtrace 3000 full";
    args[10] = NULL;

    ExecVP(args, xatoi_u(UID.c_str()), pBacktrace);

    free(args[5]);
    free(args[7]);
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

static std::string run_unstrip_n(const char *pDebugDumpDir)
{
    std::string UID;
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_UID, UID);
    }

    char* args[4];
    args[0] = (char*)"eu-unstrip";
    args[1] = xasprintf("--core=%s/"FILENAME_COREDUMP, pDebugDumpDir);
    args[2] = (char*)"-n";
    args[3] = NULL;

    std::string output;
    ExecVP(args, xatoi_u(UID.c_str()), output);

    free(args[1]);

    return output;
}

#if 0
/* older code */
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
static void InstallDebugInfos(const char *pDebugDumpDir, std::string& build_ids)
{
    log("Getting module names, file names, build IDs from core file");
    std::string unstrip_list = run_unstrip_n(pDebugDumpDir);

    log("Builting list of missing debuginfos");
    // lines look like this:
    // 0x400000+0x209000 23c77451cf6adff77fc1f5ee2a01d75de6511dda@0x40024c - - [exe]
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
        /* endsp points to 2nd space in the line now*/

        /* This filters out linux-vdso.so, among others */
        if (strstr(endsp, "[exe]") == NULL && endsp[1] != '/')
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
            build_ids += "build-id ";
            build_ids += word2;
            build_ids += file_exists ? " (debuginfo present)\n" : " (debuginfo absent)\n";
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

    update_client(_("Starting debuginfo installation"));

    int pipein[2], pipeout[2];
    xpipe(pipein);
    xpipe(pipeout);

    fflush(NULL);
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
/* Honestly, I do not know what is worse, pk-debuginfo-install or debuginfo-install:

# pk-debuginfo-install -y -- coreutils-7.2-4.fc11
1. Getting sources list...OK. Found 16 enabled and 23 disabled sources.
2. Finding debugging sources...OK. Found 0 disabled debuginfo repos.
3. Enabling debugging sources...OK. Enabled 0 debugging sources.
4. Finding debugging packages...Failed to find the package : more than one package found for
Failed to find the package : more than one package found for
FAILED. Found no packages to install.
5. Disabling sources previously enabled...OK. Disabled 0 debugging sources.

:( FAIL!

# debuginfo-install -y -- coreutils-7.2-4.fc11
Loaded plugins: refresh-packagekit
Another application is holding the yum lock, cannot continue

:( FAIL!

# debuginfo-install -y -- coreutils-7.2-4.fc11
(second time in a row - it worked)

*/
        /* log() goes to stderr/syslog, it's ok to use it here */
        VERB1 log("Executing: %s %s %s %s", "pk-debuginfo-install", "-y", "--", package.c_str());
        execlp("pk-debuginfo-install", "pk-debuginfo-install", "-y", "--", package.c_str(), NULL);
        /* fall back */
        VERB1 log("Executing: %s %s %s %s", "debuginfo-install", "-y", "--", package.c_str());
        execlp("debuginfo-install", "debuginfo-install", "-y", "--", package.c_str(), NULL);
        exit(1);
    }

    close(pipein[0]);
    close(pipeout[1]);

    /* Should not be needed (we use -y option), but just in case: */
    safe_write(pipein[1], "y\n", sizeof("y\n")-1);
    close(pipein[1]);

    FILE *pipeout_fp = fdopen(pipeout[0], "r");
    if (pipeout_fp == NULL) /* never happens */
    {
        close(pipeout[0]);
        waitpid(child, NULL, 0);
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
        strchrnul(buff, '\n')[0] = '\0';

        log("%s", buff);
        update_client("%s", buff); /* maybe only if buff != ""? */

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
            waitpid(child, NULL, 0);
            throw CABRTException(EXCEP_PLUGIN, "%s: can't install debuginfos for %s", __func__, pPackage);
        }
#endif
    }

    fclose(pipeout_fp);
    waitpid(child, NULL, 0);
}
#endif
/* Needs gdb feature from here: https://bugzilla.redhat.com/show_bug.cgi?id=528668
 * It is slated to be in F12/RHEL6.
 */
static void InstallDebugInfos(const char *pDebugDumpDir, std::string& build_ids)
{
    update_client(_("Starting debuginfo installation"));

    int pipeout[2]; //TODO: can we use ExecVP?
    xpipe(pipeout);

    fflush(NULL);
    pid_t child = fork();
    if (child < 0)
    {
        /*close(pipeout[0]); - why bother */
        /*close(pipeout[1]); */
        perror_msg_and_die("fork");
    }
    if (child == 0)
    {
        close(pipeout[0]);
        xmove_fd(pipeout[1], STDOUT_FILENO);
        xmove_fd(xopen("/dev/null", O_RDONLY), STDIN_FILENO);
        /* Not a good idea, we won't see any error messages */
        /*close(STDERR_FILENO);*/

        setsid();

        char *coredump = xasprintf("%s/"FILENAME_COREDUMP, pDebugDumpDir);
        /* SELinux guys are not happy with /tmp, using /var/run/abrt */
        char *tempdir = xasprintf(LOCALSTATEDIR"/run/abrt/tmp-%lu-%lu", (long)getpid(), (long)time(NULL));
        /* log() goes to stderr/syslog, it's ok to use it here */
        VERB1 log("Executing: %s %s %s %s", "abrt-debuginfo-install", coredump, tempdir, DEBUGINFO_CACHE_DIR);
        execlp("abrt-debuginfo-install", "abrt-debuginfo-install", coredump, tempdir, DEBUGINFO_CACHE_DIR, NULL);
        exit(1);
    }

    close(pipeout[1]);

    FILE *pipeout_fp = fdopen(pipeout[0], "r");
    if (pipeout_fp == NULL) /* never happens */
    {
        close(pipeout[0]);
        waitpid(child, NULL, 0);
        return;
    }

    char buff[1024];
    while (fgets(buff, sizeof(buff), pipeout_fp))
    {
        strchrnul(buff, '\n')[0] = '\0';

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
            log("%s", buff);
            update_client("%s", buff);
        }
    }

    fclose(pipeout_fp);
    waitpid(child, NULL, 0);
}

static double get_dir_size(const char *dirname, std::string *worst_file, double *maxsz)
{
    DIR *dp = opendir(dirname);
    if (dp == NULL)
        return 0;

    struct dirent *ep;
    struct stat stats;
    double size = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        if (dot_or_dotdot(ep->d_name))
            continue;
        std::string dname = concat_path_file(dirname, ep->d_name);
        if (lstat(dname.c_str(), &stats) != 0)
            continue;
        if (S_ISDIR(stats.st_mode))
        {
            double sz = get_dir_size(dname.c_str(), worst_file, maxsz);
            size += sz;
        }
        else if (S_ISREG(stats.st_mode))
        {
            double sz = stats.st_size;
            size += sz;

            if (worst_file)
            {
                /* Calculate "weighted" size and age
                 * w = sz_kbytes * age_mins */
                sz /= 1024;
                long age = (time(NULL) - stats.st_mtime) / 60;
                if (age > 0)
                    sz *= age;

                if (sz > *maxsz)
                {
                    *maxsz = sz;
                    *worst_file = dname;
                }
            }
        }
    }
    closedir(dp);
    return size;
}

static void trim_debuginfo_cache(unsigned max_mb)
{
    while (1)
    {
        std::string worst_file;
        double maxsz = 0;
        double cache_sz = get_dir_size(DEBUGINFO_CACHE_DIR, &worst_file, &maxsz);
        if (cache_sz / (1024 * 1024) < max_mb)
            break;
        VERB1 log("%s is %.0f bytes (over %u MB), deleting '%s'",
                DEBUGINFO_CACHE_DIR, cache_sz, max_mb, worst_file.c_str());
        if (unlink(worst_file.c_str()) != 0)
            perror_msg("Can't unlink '%s'", worst_file.c_str());
    }
}

std::string CAnalyzerFirefox::GetLocalUUID(const char *pDebugDumpDir)
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

std::string CAnalyzerFirefox::GetGlobalUUID(const char *pDebugDumpDir)
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

static bool DebuginfoCheckPolkit(uid_t uid)
{
    fflush(NULL);
    int child_pid = fork();
    if (child_pid < 0)
    {
        perror_msg_and_die("fork");
    }
    if (child_pid == 0)
    {
        //child
        xsetreuid(uid, uid);
        PolkitResult result = polkit_check_authorization(getpid(),
                 "org.fedoraproject.abrt.install-debuginfos");
        exit(result != PolkitYes); //exit 1 (failure) if not allowed
    }

    //parent
    int status;
    if (waitpid(child_pid, &status, 0) > 0
     && WIFEXITED(status)
     && WEXITSTATUS(status) == 0
    ) {
        return true; //authorization OK
    }
    log("UID %d is not authorized to install debuginfos", uid);
    return false;
}

void CAnalyzerFirefox::CreateReport(const char *pDebugDumpDir, int force)
{
    std::string package;
    std::string backtrace;
    std::string UID;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    if (!m_bBacktrace)
    {
        return;
    }

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
    if (m_bInstallDebugInfo && DebuginfoCheckPolkit(xatoi_u(UID.c_str()))) {
        if (m_nDebugInfoCacheMB > 0)
            trim_debuginfo_cache(m_nDebugInfoCacheMB);
        InstallDebugInfos(pDebugDumpDir, build_ids);
    }
    else
    {
        VERB1 log(_("Skipping debuginfo installation"));
    }

    GetBacktrace(pDebugDumpDir, backtrace);

    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_BACKTRACE, (build_ids + backtrace).c_str());
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_MEMORYMAP, "memory map of the crashed C/C++ application, not implemented yet");
    }
    dd.SaveText(FILENAME_RATING, to_string(rate_backtrace(backtrace.c_str())).c_str());
    dd.Close();
}

void CAnalyzerFirefox::Init()
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

void CAnalyzerFirefox::DeInit()
{
    std::ofstream fOutCorePattern;
    fOutCorePattern.open(CORE_PATTERN_IFACE);
    if (fOutCorePattern.is_open())
    {
        fOutCorePattern << m_sOldCorePattern << std::endl;
        fOutCorePattern.close();
    }
}

void CAnalyzerFirefox::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("Backtrace");
    if (it != end)
    {
        m_bBacktrace = string_to_bool(it->second.c_str());
    }
    it = pSettings.find("MemoryMap");
    if (it != end)
    {
        m_bMemoryMap = string_to_bool(it->second.c_str());
    }
    it = pSettings.find("DebugInfo");
    if (it != end)
    {
        m_sDebugInfo = it->second;
    }
    it = pSettings.find("DebugInfoCacheMB");
    if (it != end)
    {
        m_nDebugInfoCacheMB = xatou(it->second.c_str());
    }
    it = pSettings.find("InstallDebugInfo");
    if (it == end) //compat, remove after 0.0.11
        it = pSettings.find("InstallDebuginfo");
    if (it != end)
    {
        m_bInstallDebugInfo = string_to_bool(it->second.c_str());
    }
}

//ok to delete?
//const map_plugin_settings_t& CAnalyzerCCpp::GetSettings()
//{
//    m_pSettings["MemoryMap"] = m_bMemoryMap ? "yes" : "no";
//    m_pSettings["DebugInfo"] = m_sDebugInfo;
//    m_pSettings["DebugInfoCacheMB"] = to_string(m_nDebugInfoCacheMB);
//    m_pSettings["InstallDebugInfo"] = m_bInstallDebugInfo ? "yes" : "no";
//
//    return m_pSettings;
//}

PLUGIN_INFO(ANALYZER,
            CAnalyzerFirefox,
            "Firefox",
            "0.0.1",
            "Firefox analyzer plugin.",
            "zprikryl@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
