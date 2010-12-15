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

PyObject *ReportError;

//static PyMethodDef module_methods[] = {
//    { "dd_opendir"           , p_dd_opendir, METH_VARARGS, NULL };
//    { "dd_create"            , p_dd_create, METH_VARARGS, NULL };
//    { "delete_crash_dump_dir", p_delete_crash_dump_dir, METH_VARARGS, NULL },
//    { NULL }
//};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_pyreport(void)
{
    PyObject* m;

    if (PyType_Ready(&p_crash_data_type) < 0)
    {
        printf("PyType_Ready(&p_crash_data_type) < 0\n");
        return;
    }
    if (PyType_Ready(&p_dump_dir_type) < 0)
    {
        printf("PyType_Ready(&p_dump_dir_type) < 0\n");
        return;
    }

    m = Py_InitModule3("_pyreport", /*module_methods:*/ NULL, "Python wrapper for libreport");
    if (m == NULL)
    {
        printf("m == NULL\n");
        return;
    }

    /* init the exception object */
    ReportError = PyErr_NewException("_pyreport.error", NULL, NULL);
    Py_INCREF(ReportError);
    PyModule_AddObject(m, "error", ReportError);

    Py_INCREF(&p_crash_data_type);
    PyModule_AddObject(m, "crash_data", (PyObject *)&p_crash_data_type);

    Py_INCREF(&p_dump_dir_type);
    PyModule_AddObject(m, "dump_dir", (PyObject *)&p_dump_dir_type);
}
