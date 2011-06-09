/*
    Copyright (C) 2011  ABRT team.
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
#ifndef REPORT_H_
#define REPORT_H_

#include "problem_data.h"

enum {
    LIBREPORT_NOWAIT      = 0,
    LIBREPORT_WAIT        = (1 << 0), /* wait for report to finish and reload the problem data */
    LIBREPORT_ANALYZE     = (1 << 1), /* run analyzers? */
                                      /* ("run reporters" is always on, has no flag (for now?)) */
    LIBREPORT_RELOAD_DATA = (1 << 3), /* reload problem data after run (needs WAIT) */
};

int report_problem_in_dir(const char *dirname, int flags);

/* Reports a problem stored in problem_data_t.
 * It's first saved to /tmp and then processed as a dump dir.
 */
int report_problem_in_memory(problem_data_t *pd, int flags);

/* Simple wrapper for trivial uses */
int report_problem(problem_data_t *pd);

#endif /* REPORT_H_ */
