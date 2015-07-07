/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#include "libabrt.h"
#include "abrt-cli-core.h"

/* It is not possible to include polkitagent.h without the following define.
 * Check out the included header file.
 */
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>

int g_cli_authenticate;

static PolkitAgentListener *s_local_polkit_agent = NULL;
static gpointer s_local_agent_handle = NULL;

/* Vector of problems: */
/* problem_data_vector[i] = { "name" = { "content", CD_FLAG_foo_bits } } */

inline problem_data_t *get_problem_data(vector_of_problem_data_t *vector, unsigned i)
{
    return (problem_data_t *)g_ptr_array_index(vector, i);
}

void free_vector_of_problem_data(vector_of_problem_data_t *vector)
{
    if (vector)
        g_ptr_array_free(vector, TRUE);
}

vector_of_problem_data_t *new_vector_of_problem_data(void)
{
    return g_ptr_array_new_with_free_func((void (*)(void*)) &problem_data_free);
}

vector_of_problem_data_t *fetch_crash_infos(void)
{
    GList *problems = get_problems_over_dbus(g_cli_authenticate);
    if (problems == ERR_PTR)
        return NULL;

    vector_of_problem_data_t *vpd = new_vector_of_problem_data();

    for (GList *iter = problems; iter; iter = g_list_next(iter))
    {
        problem_data_t *problem_data = get_full_problem_data_over_dbus((const char *)(iter->data));
        if (problem_data == ERR_PTR)
            continue;

        g_ptr_array_add(vpd, problem_data);
    }

    return vpd;
}


static bool isxdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9')
            if ((*str|0x20) < 'a' || (*str|0x20) > 'f')
                return false;
        str++;
    } while (*str);
    return true;
}

char *find_problem_by_hash(const char *hash, GList *problems)
{
    unsigned hash_len = strlen(hash);
    if (!isxdigit_str(hash) || hash_len < 5)
        return NULL;

    char *found_name = NULL;
    for (GList *iter = problems; iter; iter = g_list_next(iter))
    {
        char hash_str[SHA1_RESULT_LEN*2 + 1];
        str_to_sha1str(hash_str, (const char *)(iter->data));
        if (strncasecmp(hash, hash_str, hash_len) == 0)
        {
            if (found_name)
                error_msg_and_die(_("'%s' identifies more than one problem directory"), hash);
            found_name = xstrdup((const char *)(iter->data));
        }
    }

    return found_name;
}

char *hash2dirname(const char *hash)
{
    /* Try loading by dirname hash */
    GList *problems = get_problems_over_dbus(g_cli_authenticate);
    if (problems == ERR_PTR)
        return NULL;

    char *found_name = find_problem_by_hash(hash, problems);

    g_list_free_full(problems, free);

    return found_name;
}

char *hash2dirname_if_necessary(const char *input)
{
    return isxdigit_str(input) ? hash2dirname(input) : xstrdup(input);
}

void initialize_polkit_agent(void)
{
    GError *error = NULL;
    PolkitSubject *subject = polkit_unix_process_new_for_owner(
                                getpid(),
                                /*start time from /proc*/0,
                                getuid());

    s_local_polkit_agent = polkit_agent_text_listener_new(NULL, &error);
    if (s_local_polkit_agent == NULL)
    {
        error_msg_and_die("polkit_agent_text_listener_new: %s (%s, %d)\n",
                error->message, g_quark_to_string (error->domain), error->code);
    }

    s_local_agent_handle = polkit_agent_listener_register(s_local_polkit_agent,
            POLKIT_AGENT_REGISTER_FLAGS_RUN_IN_THREAD, subject, NULL, NULL, &error);
    if (s_local_agent_handle == NULL)
    {
        error_msg_and_die("polkit_agent_listener_register: %s (%s, %d)\n",
                error->message, g_quark_to_string (error->domain), error->code);
    }

    g_object_unref(subject);
}

void uninitialize_polkit_agent(void)
{
    if (s_local_agent_handle != NULL)
        polkit_agent_listener_unregister(s_local_agent_handle);

    if (s_local_polkit_agent != NULL)
        g_object_unref(s_local_polkit_agent);
}
