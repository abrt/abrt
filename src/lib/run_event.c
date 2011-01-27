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
#include <glob.h>
#include "abrtlib.h"

struct run_event_state *new_run_event_state()
{
    return (struct run_event_state*)xzalloc(sizeof(struct run_event_state));
}

void free_run_event_state(struct run_event_state *state)
{
    free(state);
}

static int run_event_helper(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event,
                const char *conf_file_name
) {
    FILE *conffile = fopen(conf_file_name, "r");
    if (!conffile)
    {
        error_msg("Can't open '%s'", conf_file_name);
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
    setenv("EVENT", event, 1);

    /* Read, match, and execute lines from abrt_event.conf */
    int retval = -1;
    struct dump_dir *dd = NULL;
    char *next_line = xmalloc_fgetline(conffile);
    while (next_line)
    {
        char *line = next_line;
        while (1)
        {
            next_line = xmalloc_fgetline(conffile);
            if (!next_line || !isblank(next_line[0]))
                break;
            char *old_line = line;
            line = xasprintf("%s\n%s", line, next_line);
            free(old_line);
            free(next_line);
        }

        /* Line has form: [VAR=VAL]... PROG [ARGS] */
        char *p = skip_whitespace(line);
        if (*p == '\0' || *p == '#')
            goto next_line; /* empty or comment line, skip */

        VERB3 log("%s: line '%s'", __func__, p);

        if (strncmp(p, "include", strlen("include")) == 0 && isblank(p[strlen("include")]))
        {
            /* include GLOB_PATTERN */
            p = skip_whitespace(p + strlen("include"));

            const char *last_slash;
            char *name_to_glob;
            if (*p != '/'
             && (last_slash = strrchr(conf_file_name, '/')) != NULL
            )
                /* GLOB_PATTERN is relative, and this include is in path/to/file.conf
                 * Construct path/to/GLOB_PATTERN:
                 */
                name_to_glob = xasprintf("%.*s%s", (int)(last_slash - conf_file_name + 1), conf_file_name, p);
            else
                /* Either GLOB_PATTERN is absolute, or this include is in file.conf
                 * (no slashes in its name). Use unchanged GLOB_PATTERN:
                 */
                name_to_glob = xstrdup(p);

            glob_t globbuf;
            memset(&globbuf, 0, sizeof(globbuf));
            VERB3 log("%s: globbing '%s'", __func__, name_to_glob);
            glob(name_to_glob, 0, NULL, &globbuf);
            free(name_to_glob);
            char **name = globbuf.gl_pathv;
            if (name) while (*name)
            {
                VERB3 log("%s: recursing into '%s'", __func__, *name);
                run_event_helper(state, dump_dir_name, event, *name);
                VERB3 log("%s: returned from '%s'", __func__, *name);
                name++;
            }
            globfree(&globbuf);
            goto next_line;
        }

        while (1) /* word loop */
        {
            char *end_word = skip_non_whitespace(p);
            char *next_word = skip_whitespace(end_word);

            /* *end_word = '\0'; - BUG, truncates command */

            /* If there is no '=' in this word... */
            char *line_val = strchr(p, '=');
            if (!line_val || line_val >= end_word)
                break; /* ...we found the start of a command */

            *end_word = '\0';

            /* Current word has VAR=VAL form. line_val => VAL */
            *line_val++ = '\0';

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
                    {
                        free(line);
                        free(next_line);
                        goto stop; /* error (note: dd_opendir logged error msg) */
                    }
                }
                real_val = malloced_val = dd_load_text_ext(dd, p, DD_FAIL_QUIETLY);
            }

            /* Does VAL match? */
            if (strcmp(real_val, line_val) != 0)
            {
                VERB3 log("var '%s': '%.*s'!='%s', skipping line",
                        p,
                        (int)(strchrnul(real_val, '\n') - real_val), real_val,
                        line_val);
                free(malloced_val);
                goto next_line; /* no */
            }
            free(malloced_val);

            /* Go to next word */
            p = next_word;
        } /* end of word loop */

        /* Don't keep dump dir locked across program runs */
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

            retval = WEXITSTATUS(status);
            if (WIFSIGNALED(status))
                retval = WTERMSIG(status) + 128;
            if (retval != 0)
                break;
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
    dd_close(dd);
    fclose(conffile);

    return retval;
}

int run_event_on_dir_name(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event
) {
    return run_event_helper(state, dump_dir_name, event, CONF_DIR"/abrt_event.conf");
}

int run_event_on_crash_data(struct run_event_state *state, crash_data_t *data, const char *event)
{
    struct dump_dir *dd = create_dump_dir_from_crash_data(data, NULL);
    if (!dd)
        return -1;
    char *dir_name = xstrdup(dd->dd_dir);
    dd_close(dd);
    int r = run_event_on_dir_name(state, dir_name, event);
    dd = dd_opendir(dir_name, 0);
    free(dir_name);
    if (dd)
    {
        g_hash_table_remove_all(data);
        load_crash_data_from_dump_dir(data, dd);
        dd_delete(dd);
    }
    return r;
}


/* TODO: very similar to run_event_helper, try to combine into one fn? */
static void list_possible_events_helper(struct strbuf *result,
                struct dump_dir *dd,
                const char *dump_dir_name,
                const char *pfx,
                const char *conf_file_name
) {
    FILE *conffile = fopen(conf_file_name, "r");
    if (!conffile)
    {
        error_msg("Can't open '%s'", conf_file_name);
        return;
    }

    /* We check "dump_dir_name == NULL" later.
     * Prevent the possibility that both dump_dir_name
     * and dd are non-NULL (which does not make sense)
     */
    if (dd)
        dump_dir_name = NULL;

    unsigned pfx_len = strlen(pfx);
    char *next_line = xmalloc_fgetline(conffile);
    while (next_line)
    {
        char *line = next_line;
        while (1)
        {
            next_line = xmalloc_fgetline(conffile);
            if (!next_line || !isblank(next_line[0]))
                break;
            char *old_line = line;
            line = xasprintf("%s\n%s", line, next_line);
            free(old_line);
            free(next_line);
        }

        /* Line has form: [VAR=VAL]... PROG [ARGS] */
        char *p = skip_whitespace(line);
        if (*p == '\0' || *p == '#')
            goto next_line; /* empty or comment line, skip */

        VERB3 log("%s: line '%s'", __func__, p);

        if (strncmp(p, "include", strlen("include")) == 0 && isblank(p[strlen("include")]))
        {
            /* include GLOB_PATTERN */
            p = skip_whitespace(p + strlen("include"));

            const char *last_slash;
            char *name_to_glob;
            if (*p != '/'
             && (last_slash = strrchr(conf_file_name, '/')) != NULL
            )
                /* GLOB_PATTERN is relative, and this include is in path/to/file.conf
                 * Construct path/to/GLOB_PATTERN:
                 */
                name_to_glob = xasprintf("%.*s%s", (int)(last_slash - conf_file_name + 1), conf_file_name, p);
            else
                /* Either GLOB_PATTERN is absolute, or this include is in file.conf
                 * (no slashes in its name). Use unchanged GLOB_PATTERN:
                 */
                name_to_glob = xstrdup(p);

            glob_t globbuf;
            memset(&globbuf, 0, sizeof(globbuf));
            VERB3 log("%s: globbing '%s'", __func__, name_to_glob);
            glob(name_to_glob, 0, NULL, &globbuf);
            free(name_to_glob);
            char **name = globbuf.gl_pathv;
            if (name) while (*name)
            {
                VERB3 log("%s: recursing into '%s'", __func__, *name);
                list_possible_events_helper(result, dd, dump_dir_name, pfx, *name);
                VERB3 log("%s: returned from '%s'", __func__, *name);
                name++;
            }
            globfree(&globbuf);
            goto next_line;
        }

        while (1) /* word loop */
        {
            char *end_word = skip_non_whitespace(p);
            char *next_word = skip_whitespace(end_word);
            *end_word = '\0';

            /* If there is no '=' in this word... */
            char *line_val = strchr(p, '=');
            if (!line_val)
                break; /* ...we found the start of a command */

            /* Current word has VAR=VAL form. line_val => VAL */
            *line_val++ = '\0';

            /* Is it EVENT? */
            if (strcmp(p, "EVENT") == 0)
            {
                if (strncmp(line_val, pfx, pfx_len) != 0)
                    goto next_line; /* prefix doesn't match */
                /* (Ab)use line to save matching "\nEVENT_VAL\n" */
                sprintf(line, "\n%s\n", line_val);
            }
            else
            {
                /* Get this name from dump dir */
                if (!dd)
                {
                    /* Without dir name to match, we assume match for this expr */
                    if (!dump_dir_name)
                        goto next_word;
                    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
                    if (!dd)
                    {
                        free(line);
                        free(next_line);
                        goto stop; /* error (note: dd_opendir logged error msg) */
                    }
                }
                char *real_val = dd_load_text_ext(dd, p, DD_FAIL_QUIETLY);
                /* Does VAL match? */
                if (strcmp(real_val, line_val) != 0)
                {
                    VERB3 log("var '%s': '%.*s'!='%s', skipping line",
                            p,
                            (int)(strchrnul(real_val, '\n') - real_val), real_val,
                            line_val);
                    free(real_val);
                    goto next_line; /* no */
                }
                free(real_val);
            }

 next_word:
            /* Go to next word */
            p = next_word;
        } /* end of word loop */

        if (line[0] == '\n' /* do we *have* saved matched "\nEVENT_VAL\n"? */
         /* and does result->buf NOT yet have VAL? */
         && strncmp(result->buf, line + 1, strlen(line + 1)) != 0
         && !strstr(result->buf, line)
        ) {
            /* Add "EVENT_VAL\n" */
            strbuf_append_str(result, line + 1);
        }

 next_line:
        free(line);
    } /* end of line loop */

 stop:
    if (dump_dir_name != NULL)
        dd_close(dd);
    fclose(conffile);
}

char *list_possible_events(struct dump_dir *dd, const char *dump_dir_name, const char *pfx)
{
    struct strbuf *result = strbuf_new();
    list_possible_events_helper(result, dd, dump_dir_name, pfx, CONF_DIR"/abrt_event.conf");
    return strbuf_free_nobuf(result);
}
