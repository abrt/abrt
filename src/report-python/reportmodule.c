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

static PyMethodDef module_methods[] = {
    /* method_name, func, flags, doc_string */
    { "dd_opendir"     , p_dd_opendir     , METH_VARARGS },
    { "dd_create"      , p_dd_create      , METH_VARARGS },
    { "delete_dump_dir", p_delete_dump_dir, METH_VARARGS },
    { NULL }
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_pyreport(void)
{
    if (PyType_Ready(&p_problem_data_type) < 0)
    {
        printf("PyType_Ready(&p_problem_data_type) < 0\n");
        return;
    }
    if (PyType_Ready(&p_dump_dir_type) < 0)
    {
        printf("PyType_Ready(&p_dump_dir_type) < 0\n");
        return;
    }
    if (PyType_Ready(&p_run_event_state_type) < 0)
    {
        printf("PyType_Ready(&p_run_event_state_type) < 0\n");
        return;
    }

    PyObject *m = Py_InitModule("_pyreport", module_methods);
    //m = Py_InitModule3("_pyreport", module_methods, "Python wrapper for libreport");
    if (!m)
    {
        printf("m == NULL\n");
        return;
    }

    /* init the exception object */
    ReportError = PyErr_NewException((char*) "_pyreport.error", NULL, NULL);
    Py_INCREF(ReportError);
    PyModule_AddObject(m, "error", ReportError);

    /* init type objects */
    Py_INCREF(&p_problem_data_type);
    PyModule_AddObject(m, "problem_data", (PyObject *)&p_problem_data_type);
    PyModule_AddObject(m, "CD_FLAG_BIN"          , Py_BuildValue("i", CD_FLAG_BIN          ));
    PyModule_AddObject(m, "CD_FLAG_TXT"          , Py_BuildValue("i", CD_FLAG_TXT          ));
    PyModule_AddObject(m, "CD_FLAG_ISEDITABLE"   , Py_BuildValue("i", CD_FLAG_ISEDITABLE   ));
    PyModule_AddObject(m, "CD_FLAG_ISNOTEDITABLE", Py_BuildValue("i", CD_FLAG_ISNOTEDITABLE));

    Py_INCREF(&p_dump_dir_type);
    PyModule_AddObject(m, "dump_dir", (PyObject *)&p_dump_dir_type);
    PyModule_AddObject(m, "DD_FAIL_QUIETLY_ENOENT"             , Py_BuildValue("i", DD_FAIL_QUIETLY_ENOENT             ));
    PyModule_AddObject(m, "DD_FAIL_QUIETLY_EACCES"             , Py_BuildValue("i", DD_FAIL_QUIETLY_EACCES             ));
    PyModule_AddObject(m, "DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE", Py_BuildValue("i", DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE));

    Py_INCREF(&p_run_event_state_type);
    PyModule_AddObject(m, "run_event_state", (PyObject *)&p_run_event_state_type);
}
