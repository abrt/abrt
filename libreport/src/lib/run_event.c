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
#include <regex.h>
#include "libreport.h"

struct run_event_state *new_run_event_state()
{
    return xzalloc(sizeof(struct run_event_state));
}

void free_run_event_state(struct run_event_state *state)
{
    if (state)
    {
        free_commands(state);
        free(state);
    }
}


/* Asynchronous command execution */

/* It is not yet clear whether we need to re-parse event config file
 * and re-check the elements in dump dir after each command.
 *
 * Consider this config file:
 *
 * EVENT=e         cmd1
 * EVENT=e foo=bar cmd2
 * EVENT=e foo=baz cmd3
 *
 * Imagine that element foo existed and was equal to bar at the beginning.
 * After cmd1, should we execute cmd2 if element foo disappeared?
 * After cmd1/2, should we execute cmd3 if element foo changed value to baz?
 *
 * We used to read entire config file and select a list of commands to execute,
 * checking all conditions in the beginning. It is a bit more simple to code up.
 *
 * This proved to be bad for use cases where, for example, post-create rule
 * for a specified package needs to run:
 *
 * EVENT=post-create
 *      abrt-action-save-package-data
 * EVENT=post-create component=mypkg
 *      my_handling
 *
 * Problem here is that "component" element is created by
 * abrt-action-save-package-data! Pre-selecting rules excludes second rule.
 *
 * Now we read entire config but do NOT select commands to execute,
 * we check conditions of every next rule *directly before its execution*.
 *
 * It's possible we'd want to switch to an algorightm which makes in unnecessary
 * to properly order rules in config files(s). Now these two rules must not
 * be reordered, or else second one won't work:
 * EVENT=post-create         echo foo >bar
 * EVENT=post-create foo=bar do_something
 * but this might be not so easy to ensure when include files are involved...
 *
 * Anyway, list of commands machinery is encapsulated in struct run_event_state,
 * and public async API:
 *      prepare_commands(state, dir, event);
 *      spawn_next_command(state, dir, event);
 *      free_commands(state);
 * does not expose the way we select rules to execute.
 */
struct rule {
    GList *conditions;
    char *command; /* never NULL */
};

static void free_rule_list(GList *rule_list)
{
    while (rule_list)
    {
        struct rule *cur_rule = rule_list->data;
        list_free_with_free(cur_rule->conditions);
        free(cur_rule->command);
        free(cur_rule);

        GList *next = rule_list->next;
        g_list_free_1(rule_list);
        rule_list = next;
    }
}

/* Stop-gap measure against infinite recursion */
#define MAX_recursion_depth 32

static GList *load_rule_list(GList *rule_list,
                const char *conf_file_name,
                unsigned recursion_depth
) {
    FILE *conffile = fopen(conf_file_name, "r");
    if (!conffile)
    {
        error_msg("Can't open '%s'", conf_file_name);
        return rule_list;
    }

    /* Read and remember rules */
    char *next_line = xmalloc_fgetline(conffile);
    while (next_line)
    {
        /* Read and concatenate all lines in a rule */
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

        char *p = skip_whitespace(line);
        if (*p == '\0' || *p == '#')
            goto next_line; /* empty or comment line, skip */

        //VERB3 log("%s: line '%s'", __func__, p);

        /* Handle "include" directive */
        if (recursion_depth < MAX_recursion_depth
         && strncmp(p, "include", strlen("include")) == 0
         && isblank(p[strlen("include")])
        ) {
            /* "include GLOB_PATTERN" */
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
            //VERB3 log("%s: globbing '%s'", __func__, name_to_glob);
            glob(name_to_glob, 0, NULL, &globbuf);
            free(name_to_glob);
            char **name = globbuf.gl_pathv;
            if (name) while (*name)
            {
                //VERB3 log("%s: recursing into '%s'", __func__, *name);
                rule_list = load_rule_list(rule_list, *name, recursion_depth + 1);
                //VERB3 log("%s: returned from '%s'", __func__, *name);
                name++;
            }
            globfree(&globbuf);
            goto next_line;
        }

        /* Rule has form: [VAR=VAL]... PROG [ARGS] */
        struct rule *cur_rule = xzalloc(sizeof(*cur_rule));

        while (1) /* word loop */
        {
            char *end_word = skip_non_whitespace(p);

            /* If there is no '=' in this word... */
            char *line_val = strchr(p, '=');
            if (!line_val || line_val >= end_word)
                break; /* ...we found the start of a command */

            cur_rule->conditions = g_list_append(cur_rule->conditions, xstrndup(p, end_word - p));

            /* Go to next word */
            p = skip_whitespace(end_word);
        } /* end of word loop */

        VERB1 log("Adding '%s'", p);
        cur_rule->command = xstrdup(p);

        rule_list = g_list_append(rule_list, cur_rule);

 next_line:
        free(line);
    } /* end of line loop */

    fclose(conffile);

    return rule_list;
}

static int regcmp_lines(char *val, const char *regex)
{
    regex_t rx;
    int r = regcomp(&rx, regex, REG_NOSUB); //TODO: and REG_EXTENDED?
    //log("REGEX:'%s':%d", regex, r);
    if (r)
    {
        //char errbuf[256];
        //size_t needsz = regerror(r, &rx, errbuf, sizeof(errbuf));
        error_msg("Bad regexp '%s'", regex); // TODO: use errbuf?
        return r;
    }

    /* Check every line */
    while (1)
    {
        char *eol = strchr(val, '\n');
        if (eol)
            *eol = '\0';
        r = regexec(&rx, val, 0, NULL, /*eflags:*/ 0);
        //log("REGCMP:'%s':%d", val, r);
        if (eol)
            *eol = '\n';
        if (r == 0 || !eol)
            break;
        val = eol + 1;
    }
    /* Here, r == 0 if match was found */
    regfree(&rx);
    return r;
}

/* Deletes rules in *pp_rule_list, starting from first (remaining) rule,
 * until it finds a rule with all conditions satisfied.
 * In this case, it deletes this rule and returns this rule's cmd.
 * Else (if it didn't find such rule), it deletes all rules and returns NULL.
 * In case of error (dump_dir can't be opened), deletes all rules and returns NULL.
 *
 * Intended usage:
 * list = load_rule_list(...);
 * while ((cmd = pop_next_command(&list, ...)) != NULL)
 *     run(cmd);
 */
static char* pop_next_command(GList **pp_rule_list,
        char **pp_event_name,    /* reports EVENT value thru this, if not NULL on entry */
        struct dump_dir **pp_dd, /* use *pp_dd for access to dump dir, if non-NULL */
        const char *dump_dir_name,
        const char *pfx,
        unsigned pfx_len
)
{
    char *command = NULL;
    struct dump_dir *dd = pp_dd ? *pp_dd : NULL;

    GList *rule_list = *pp_rule_list;
    while (rule_list)
    {
        struct rule *cur_rule = rule_list->data;

        GList *condition = cur_rule->conditions;
        while (condition)
        {
            const char *cond_str = condition->data;
            const char *eq_sign = strchr(cond_str, '=');

            /* Is it "EVENT=foo"? */
            if (strncmp(cond_str, "EVENT=", 6) == 0)
            {
                if (strncmp(eq_sign + 1, pfx, pfx_len) != 0)
                    goto next_rule; /* prefix doesn't match */
                if (pp_event_name)
                {
                    free(*pp_event_name);
                    *pp_event_name = xstrdup(eq_sign + 1);
                }
            }
            else
            {
                /* Read from dump dir and compare */
                if (!dd)
                {
                    /* Without dir to match, we assume match for all conditions */
                    if (!dump_dir_name)
                        goto next_cond;
                    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
                    if (!dd)
                    {
                        free_rule_list(rule_list);
                        *pp_rule_list = NULL;
                        goto ret; /* error (note: dd_opendir logged error msg) */
                    }
                }
                /* Is it "VAR~=REGEX"? */
                int regex = (eq_sign > cond_str && eq_sign[-1] == '~');
                char *var_name = xstrndup(cond_str, eq_sign - cond_str - regex);
                char *real_val = dd_load_text_ext(dd, var_name, DD_FAIL_QUIETLY_ENOENT);
                free(var_name);
                int vals_differ = regex ? regcmp_lines(real_val, eq_sign + 1) : strcmp(real_val, eq_sign + 1);
                free(real_val);

                /* Do values match? */
                if (vals_differ) /* no */
                {
                    //VERB3 log("var '%s': '%.*s'!='%s', skipping line",
                    //        p,
                    //        (int)(strchrnul(real_val, '\n') - real_val), real_val,
                    //        eq_sign);
                    goto next_rule;
                }
            }
 next_cond:
            /* We are here if current condition is satisfied */

            condition = condition->next;
        }
        /* We are here if all conditions are satisfied */

        command = cur_rule->command;

 next_rule:
        *pp_rule_list = rule_list->next;
        g_list_free_1(rule_list);

        list_free_with_free(cur_rule->conditions);
        /*free(cur_rule->command); - WRONG! we might be returning it! */
        if (command)
        {
            /* We found rule to run, return it */
            free(cur_rule);
            break;
        }
        free(cur_rule->command); /* _now_ it is ok */
        free(cur_rule);

        rule_list = *pp_rule_list;
    } /* while (rule_list) */

 ret:
    if (pp_dd)
        *pp_dd = dd;
    else
        dd_close(dd);
    return command;
}

void free_commands(struct run_event_state *state)
{
    free_rule_list(state->rule_list);
    state->rule_list = NULL;
    state->command_out_fd = -1;
    state->command_pid = 0;
}

int prepare_commands(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event
) {
    free_commands(state);

    state->children_count = 0;

    GList *rule_list = load_rule_list(NULL, CONF_DIR"/report_event.conf", /*recursion_depth:*/ 0);
    state->rule_list = rule_list;
    return rule_list != NULL;
}

int spawn_next_command(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event
) {
    char *cmd = pop_next_command(&state->rule_list,
                NULL,          /* don't return event_name */
                NULL,          /* NULL &dd: we match by... */
                dump_dir_name, /* ...dirname */
                event, strlen(event)+1 /* for this event name exactly (not prefix) */
    );
    if (!cmd)
        return -1;

    /* We count it even if fork fails. The counter isn't meant
     * to count *successful* forks, it is meant to let caller know
     * whether the event we run has *any* handlers configured, or not.
     */
    state->children_count++;

    VERB1 log("Executing '%s'", cmd);

    /* Export some useful environment variables for children */
    char *env_vec[3];
    /* Just exporting dump_dir_name isn't always ok: it can be "."
     * and some children want to cd to other directory but still
     * be able to find dump directory by using $DUMP_DIR...
     */
    char *full_name = realpath(dump_dir_name, NULL);
    env_vec[0] = xasprintf("DUMP_DIR=%s", (full_name ? full_name : dump_dir_name));
    free(full_name);
    env_vec[1] = xasprintf("EVENT=%s", event);
    env_vec[2] = NULL;

    char *argv[4];
    argv[0] = (char*)"/bin/sh"; // TODO: honor $SHELL?
    argv[1] = (char*)"-c";
    argv[2] = cmd;
    argv[3] = NULL;

    int pipefds[2];
    state->command_pid = fork_execv_on_steroids(
                EXECFLG_INPUT_NUL + EXECFLG_OUTPUT + EXECFLG_ERR2OUT,
                argv,
                pipefds,
                /* env_vec: */ env_vec,
                /* dir: */ dump_dir_name,
                /* uid(unused): */ 0
    );
    state->command_out_fd = pipefds[0];

    free(env_vec[0]);
    free(env_vec[1]);
    free(cmd);

    return 0;
}


/* Synchronous command execution:
 */
int run_event_on_dir_name(struct run_event_state *state,
                const char *dump_dir_name,
                const char *event
) {
    prepare_commands(state, dump_dir_name, event);

    /* Execute every command in shell */

    int retval = 0;
    while (spawn_next_command(state, dump_dir_name, event) >= 0)
    {
        /* Consume log from stdout */
        FILE *fp = fdopen(state->command_out_fd, "r");
        if (!fp)
            die_out_of_memory();
        char *buf;
        while ((buf = xmalloc_fgetline(fp)) != NULL)
        {
            if (state->logging_callback)
                buf = state->logging_callback(buf, state->logging_param);
            free(buf);
        }
        fclose(fp); /* Got EOF, close. This also closes state->command_out_fd */

        /* Wait for child to actually exit, collect status */
        int status;
        waitpid(state->command_pid, &status, 0);

        retval = WEXITSTATUS(status);
        if (WIFSIGNALED(status))
            retval = WTERMSIG(status) + 128;
        if (retval != 0)
            break;

        if (state->post_run_callback)
        {
            retval = state->post_run_callback(dump_dir_name, state->post_run_param);
            if (retval != 0)
                break;
        }
    }

    free_commands(state);

    return retval;
}

int run_event_on_problem_data(struct run_event_state *state, problem_data_t *data, const char *event)
{
    state->children_count = 0;

    struct dump_dir *dd = create_dump_dir_from_problem_data(data, NULL);
    if (!dd)
        return -1;
    char *dir_name = xstrdup(dd->dd_dirname);
    dd_close(dd);

    int r = run_event_on_dir_name(state, dir_name, event);

    g_hash_table_remove_all(data);
    dd = dd_opendir(dir_name, /*flags:*/ 0);
    free(dir_name);
    if (dd)
    {
        load_problem_data_from_dump_dir(data, dd, NULL);
        dd_delete(dd);
    }

    return r;
}

char *list_possible_events(struct dump_dir *dd, const char *dump_dir_name, const char *pfx)
{
    struct strbuf *result = strbuf_new();

    GList *rule_list = load_rule_list(NULL, CONF_DIR"/report_event.conf", /*recursion_depth:*/ 0);

    unsigned pfx_len = strlen(pfx);
    for (;;)
    {
        /* Retrieve each cmd, and fetch its EVENT=foo value */
        char *event_name = NULL;
        char *cmd = pop_next_command(&rule_list,
                &event_name,       /* return event_name */
                (dd ? &dd : NULL), /* match this dd... */
                dump_dir_name,     /* ...or if NULL, this dirname */
                pfx, pfx_len       /* for events with this prefix */
        );
        if (!cmd)
            break;
        free(cmd);

        if (event_name)
        {
            /* Append "EVENT\n" - only if it is not there yet */
            unsigned e_len = strlen(event_name);
            char *p = result->buf;
            while (p && *p)
            {
                if (strncmp(p, event_name, e_len) == 0 && p[e_len] == '\n')
                    goto skip; /* This event is already in the result */
                p = strchr(p, '\n');
                if (p)
                    p++;
            }
            strbuf_append_strf(result, "%s\n", event_name);
 skip:
            free(event_name);
        }
    }

    return strbuf_free_nobuf(result);
}
