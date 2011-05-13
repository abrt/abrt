/*
    Copyright (C) 2009  Abrt team.
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
#ifndef REPORT_H_
#define REPORT_H_

#include "problem_data.h"

/* analyzes AND reports a problem saved on disk
 * - takes user through all the steps in reporting wizard
 */
int analyze_and_report_dir(const char* dirname);

/* analyzes AND reports a problem stored in problem_data_t
 * it's first saved to /tmp and then processed as a dump_dir
 * - takes user through all the steps in reporting wizard
 */
int analyze_and_report(problem_data_t *pd);

/* reports a problem saved on disk
 * - shows only reporter selector and progress
*/
int report_dir(const char* dirname);

/* to report a problem stored in memory */
int report(problem_data_t *pd);

#endif /* REPORT_H_ */
