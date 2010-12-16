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

/* exception object */
extern PyObject *ReportError;

/* type objects */
extern PyTypeObject p_crash_data_type;
extern PyTypeObject p_dump_dir_type;
extern PyTypeObject p_run_event_state_type;

/* module-level functions */
PyObject *p_dd_opendir(PyObject *module, PyObject *args);
PyObject *p_dd_create(PyObject *module, PyObject *args);

/* python objects' struct defs */
typedef struct {
    PyObject_HEAD
    struct dump_dir *dd;
} p_dump_dir;
