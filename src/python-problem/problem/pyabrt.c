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
#include "libabrt.h"
#include "problem_data.h"
#include "common.h"

/* C: void notify_new_path(const char *path); */
PyObject *p_notify_new_path(PyObject *pself, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
    {
        return NULL;
    }
    notify_new_path(path);
    Py_RETURN_NONE;
}
