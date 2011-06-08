/*
    Copyright (C) 2010  Abrt team.
    Copyright (C) 2010  RedHat inc.

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

#include "common.h"

/* C: int report_problem_in_dir(const char *dirname, int flags); */
PyObject *p_report_problem_in_dir(PyObject *pself, PyObject *args)
{
    const char *dirname;
    int flags;
    if (!PyArg_ParseTuple(args, "si", &dirname, &flags))
    {
        return NULL;
    }
    int r = report_problem_in_dir(dirname, flags);
    return Py_BuildValue("i", r);
}

/* C: int report_problem_in_memory(problem_data_t *pd, int flags); */
PyObject *p_report_problem_in_memory(PyObject *pself, PyObject *args)
{
    p_problem_data *pd;
    int flags;
    if (!PyArg_ParseTuple(args, "O!i", &p_problem_data_type, &pd, &flags))
    {
        return NULL;
    }
    int r = report_problem_in_memory(pd->cd, flags);
    return Py_BuildValue("i", r);
}

/* C: int report_problem(problem_data_t *pd); */
PyObject *p_report_problem(PyObject *pself, PyObject *args)
{
    p_problem_data *pd;
    if (!PyArg_ParseTuple(args, "O!", &p_problem_data_type, &pd))
    {
        return NULL;
    }
    int r = report_problem(pd->cd);
    return Py_BuildValue("i", r);
}
