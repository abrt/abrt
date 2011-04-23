/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
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
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "problem_data.h"
#include "dump_dir.h"
#include "run_event.h"

static char *do_log(char *log_line, void *param)
{
    printf("%s\n", log_line);
    return log_line;
}

int main(int argc, char** argv)
{
    problem_data_t *problem_data = new_problem_data();

    add_to_problem_data(problem_data, "analyzer", "wow");
    const char *event = "report";

    struct dump_dir *dd = create_dump_dir_from_problem_data(problem_data, "/tmp");
    free_problem_data(problem_data);
    if (!dd)
        return 1;
    char *dir_name = strdup(dd->dd_dirname);
    dd_close(dd);

    printf("Temp dump dir: '%s'\n", dir_name);

    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log;
    int r = run_event_on_dir_name(run_state, dir_name, event);
    if (r == 0 && run_state->children_count == 0)
        printf("No actions are found for event '%s'\n", event);
    free_run_event_state(run_state);

//    delete_dump_dir(dir_name);
    free(dir_name);

    return 0;
}
