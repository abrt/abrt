/*
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
#include "parse_options.h"

#define PROGNAME "abrt-action-analyze-c"

static void create_hash(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *pInput)
{
    unsigned char hash_bytes[SHA1_RESULT_LEN];

    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, pInput, strlen(pInput));
    sha1_end(&sha1ctx, hash_bytes);

    unsigned len = SHA1_RESULT_LEN;
    unsigned char *s = hash_bytes;
    char *d = hash_str;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';
    //log("hash:%s str:'%s'", hash_str, pInput);
}

static char *run_unstrip_n(const char *dump_dir_name, unsigned timeout_sec)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    char *uid_str = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    dd_close(dd);
    uid_t uid = -1L;
    if (uid_str)
    {
        uid = xatoi_positive(uid_str);
        free(uid_str);
        if (uid == geteuid())
        {
            uid = -1L; /* no need to setuid/gid if we are already under right uid */
        }
    }

    int flags = EXECFLG_INPUT_NUL | EXECFLG_OUTPUT | EXECFLG_SETSID | EXECFLG_QUIET;
    if (uid != (uid_t)-1L)
        flags |= EXECFLG_SETGUID;
    VERB1 flags &= ~EXECFLG_QUIET;
    int pipeout[2];
    char* args[4];
    args[0] = (char*)"eu-unstrip";
    args[1] = xasprintf("--core=%s/"FILENAME_COREDUMP, dump_dir_name);
    args[2] = (char*)"-n";
    args[3] = NULL;
    pid_t child = fork_execv_on_steroids(flags, args, pipeout, /*env_vec:*/ NULL, /*dir:*/ NULL, uid);
    free(args[1]);

    /* Bugs in unstrip or corrupted coredumps can cause it to enter infinite loop.
     * Therefore we have a (largish) timeout, after which we kill the child.
     */
    int t = time(NULL); /* int is enough, no need to use time_t */
    int endtime = t + timeout_sec;
    struct strbuf *buf_out = strbuf_new();
    while (1)
    {
        int timeout = endtime - t;
        if (timeout < 0)
        {
            kill(child, SIGKILL);
            strbuf_append_strf(buf_out, "\nTimeout exceeded: %u seconds, killing %s\n", timeout_sec, args[0]);
            break;
        }

        /* We don't check poll result - checking read result is enough */
        struct pollfd pfd;
        pfd.fd = pipeout[0];
        pfd.events = POLLIN;
        poll(&pfd, 1, timeout * 1000);

        char buff[1024];
        int r = read(pipeout[0], buff, sizeof(buff) - 1);
        if (r <= 0)
            break;
        buff[r] = '\0';
        strbuf_append_str(buf_out, buff);
        t = time(NULL);
    }
    close(pipeout[0]);

    /* Prevent having zombie child process */
    int status;
    waitpid(child, &status, 0);

    if (status != 0)
    {
        /* unstrip didnt exit with exit code 0 */
        strbuf_free(buf_out);
        return NULL;
    }

    return strbuf_free_nobuf(buf_out);
}

static void trim_unstrip_output(char *result, const char *unstrip_n_output)
{
    // lines look like this:
    // 0x400000+0x209000 23c77451cf6adff77fc1f5ee2a01d75de6511dda@0x40024c - - [exe]
    // 0x400000+0x209000 ab3c8286aac6c043fd1bb1cc2a0b88ec29517d3e@0x40024c /bin/sleep /usr/lib/debug/bin/sleep.debug [exe]
    // 0x7fff313ff000+0x1000 389c7475e3d5401c55953a425a2042ef62c4c7df@0x7fff313ff2f8 . - linux-vdso.so.1
    //                ^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // we drop everything except the marked part ^

    char *dst = result;
    const char *line = unstrip_n_output;
    while (*line)
    {
        const char *eol = strchrnul(line, '\n');
        const char *plus = (char*)memchr(line, '+', eol - line);
        if (plus)
        {
            while (++plus < eol && *plus != '@')
            {
                if (!isspace(*plus))
                {
                    *dst++ = *plus;
                }
            }
        }
        if (*eol != '\n') break;
        line = eol + 1;
    }
    *dst = '\0';
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -d DIR\n"
        "\n"
        "Calculates and saves UUID of coredump in dump directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    /* Run unstrip -n and trim its output, leaving only sizes and build ids */

    char *unstrip_n_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
    if (!unstrip_n_output)
        return 1; /* bad dump_dir_name, can't run unstrip, etc... */
    /* modifies unstrip_n_output in-place: */
    trim_unstrip_output(unstrip_n_output, unstrip_n_output);

    /* Hash package + executable + unstrip_n_output and save it as UUID */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    char *package = dd_load_text(dd, FILENAME_PACKAGE);
    /* Package variable has "firefox-3.5.6-1.fc11[.1]" format */
    /* Remove distro suffix and maybe least significant version number */
    char *p = package;
    while (*p)
    {
        if (*p == '.' && (p[1] < '0' || p[1] > '9'))
        {
            /* We found "XXXX.nondigitXXXX", trim this part */
            *p = '\0';
            break;
        }
        p++;
    }
    char *first_dot = strchr(package, '.');
    if (first_dot)
    {
        char *last_dot = strrchr(first_dot, '.');
        if (last_dot != first_dot)
        {
            /* There are more than one dot: "1.2.3"
             * Strip last part, we don't want to distinquish crashes
             * in packages which differ only by minor release number.
             */
            *last_dot = '\0';
        }
    }

    char *string_to_hash = xasprintf("%s%s%s", package, executable, unstrip_n_output);
    /*free(package);*/
    /*free(executable);*/
    /*free(unstrip_n_output);*/

    char hash_str[SHA1_RESULT_LEN*2 + 1];
    create_hash(hash_str, string_to_hash);

    dd_save_text(dd, FILENAME_UUID, hash_str);
    dd_close(dd);

    return 0;
}
