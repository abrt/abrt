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
#include <Python.h>

#include "dump_dir.h"
#include "problem_data.h"
#include "run_event.h"
#include "report.h"

/* exception object */
extern PyObject *ReportError;

/* type objects */
extern PyTypeObject p_problem_data_type;
extern PyTypeObject p_dump_dir_type;
extern PyTypeObject p_run_event_state_type;

/* python objects' struct defs */
typedef struct {
    PyObject_HEAD
    struct dump_dir *dd;
} p_dump_dir;

typedef struct {
    PyObject_HEAD
    problem_data_t *cd;
} p_problem_data;

/* module-level functions */
/* for include/report/dump_dir.h */
PyObject *p_dd_opendir(PyObject *module, PyObject *args);
PyObject *p_dd_create(PyObject *module, PyObject *args);
PyObject *p_delete_dump_dir(PyObject *pself, PyObject *args);
/* for include/report/report.h */
PyObject *p_report_problem_in_dir(PyObject *pself, PyObject *args);
PyObject *p_report_problem_in_memory(PyObject *pself, PyObject *args);
PyObject *p_report_problem(PyObject *pself, PyObject *args);
