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

int run_event(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event
) {
    FILE *conffile = fopen(CONF_DIR"/abrt_action.conf", "r");
    if (!conffile)
    {
        error_msg("Can't open '%s'", CONF_DIR"/abrt_action.conf");
        return 1;
    }
    close_on_exec_on(fileno(conffile));

    /* Export some useful environment variables for children */
    /* Just exporting dump_dir_name isn't always ok: it can be "."
     * and some children want to cd to other directory but still
     * be able to find dump directory by using $DUMP_DIR...
     */
    char *full_name = realpath(dump_dir_name, NULL);
    setenv("DUMP_DIR", (full_name ? full_name : dump_dir_name), 1);
    free(full_name);
    /*setenv("EVENT", event, 1); - is this useful for children to know? */

    /* Read, match, and execute lines from abrt_action.conf */
    int retval = 0;
    struct dump_dir *dd = NULL;
    char *line;
    while ((line = xmalloc_fgetline(conffile)) != NULL)
    {
        /* Line has form: [VAR=VAL]... PROG [ARGS] */
        char *p = skip_whitespace(line);
        if (*p == '\0' || *p == '#')
            goto next_line; /* empty or comment line, skip */

        VERB3 log("line '%s'", p);

        while (1) /* word loop */
        {
            /* If there is no '=' in this word... */
            char *next_word = skip_whitespace(skip_non_whitespace(p));
            char *needed_val = strchr(p, '=');
            if (!needed_val || needed_val >= next_word)
                break; /* ...we found the start of a command */

            /* Current word has VAR=VAL form. needed_val => VAL */
            *needed_val++ = '\0';

            const char *real_val;
            char *malloced_val = NULL;

            /* Is it EVENT? */
            if (strcmp(p, "EVENT") == 0)
                real_val = event;
            else
            {
                /* Get this name from dump dir */
                if (!dd)
                {
                    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
                    if (!dd)
                        goto stop; /* error (note: dd_opendir logged error msg) */
                }
                real_val = malloced_val = dd_load_text(dd, p);
            }

            /* Does VAL match? */
            unsigned len = strlen(real_val);
            bool match = (strncmp(real_val, needed_val, len) == 0
                         && (needed_val[len] == ' ' || needed_val[len] == '\t'));
            if (!match)
            {
                VERB3 log("var '%s': '%s'!='%s', skipping line", p, real_val, needed_val);
                free(malloced_val);
                goto next_line; /* no */
            }
            free(malloced_val);

            /* Go to next word */
            p = next_word;
        } /* end of word loop */

        dd_close(dd);
        dd = NULL;

        /* We found matching line, execute its command(s) in shell */
        {
            VERB1 log("Executing '%s'", p);

            /* /bin/sh -c 'cmd [args]' NULL */
            char *argv[4];
            char **pp = argv;
            *pp++ = (char*)"/bin/sh";
            *pp++ = (char*)"-c";
            *pp++ = (char*)p;
            *pp = NULL;
            int pipefds[2];
            pid_t pid = fork_execv_on_steroids(EXECFLG_INPUT_NUL + EXECFLG_OUTPUT + EXECFLG_ERR2OUT,
                        argv,
                        pipefds,
                        /* unsetenv_vec: */ NULL,
                        /* dir: */ dump_dir_name,
                        /* uid(unused): */ 0
            );
            free(line);
            line = NULL;

            /* Consume log from stdout */
            FILE *fp = fdopen(pipefds[0], "r");
            if (!fp)
                die_out_of_memory();
            char *buf;
            while ((buf = xmalloc_fgetline(fp)) != NULL)
            {
                if (state->logging_callback)
                    buf = state->logging_callback(buf, state->logging_param);
                free(buf);
            }
            fclose(fp); /* Got EOF, close. This also closes pipefds[0] */
            /* Wait for child to actually exit, collect status */
            int status;
            waitpid(pid, &status, 0);

            if (status != 0)
            {
                retval = WEXITSTATUS(status);
                if (WIFSIGNALED(status))
                    retval = WTERMSIG(status) + 128;
                break;
            }
        }

        if (state->post_run_callback)
        {
            retval = state->post_run_callback(dump_dir_name, state->post_run_param);
            if (retval != 0)
                break;
        }

 next_line:
        free(line);
    } /* end of line loop */

 stop:
    free(line);
    dd_close(dd);
    fclose(conffile);

    return retval;
}
