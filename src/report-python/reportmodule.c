#include <Python.h>
#include "py_crash_dump.h"

PyObject *ReportError;

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_pyreport(void)
{
    PyObject* m;

    if (PyType_Ready(&p_crash_data_type) < 0)
    {
        printf("PyType_Ready(&p_crash_data_type) < 0");
        return;
    }

    m = Py_InitModule3("_pyreport", module_methods, "Python wrapper around crash_data_t");
    if (m == NULL)
    {
        printf("m == NULL");
        return;
    }

    /* init the exception object */
    ReportError = PyErr_NewException("_pyreport.error", NULL, NULL);
    Py_INCREF(ReportError);
    PyModule_AddObject(m, "error", ReportError);

    Py_INCREF(&p_crash_data_type);
    PyModule_AddObject(m, "crash_data", (PyObject *)&p_crash_data_type);
}
