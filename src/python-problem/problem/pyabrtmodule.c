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

#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT PyMODINIT_FUNC PyInit__py3abrt(void)
  #define MOD_DEF(ob, name, doc, methods) \
            static struct PyModuleDef moduledef = { \
                          PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT void init_pyabrt(void)
  #define MOD_DEF(ob, name, doc, methods) \
            ob = Py_InitModule3(name, methods, doc);
#endif

static char module_doc[] = "ABRT utilities";

static PyMethodDef module_methods[] = {
    /* method_name, func, flags, doc_string */
    /* for include/client.h */
    { "notify_new_path"           , p_notify_new_path         , METH_VARARGS },
    { "load_conf_file"            , p_load_conf_file          , METH_VARARGS },
    { "load_plugin_conf_file"     , p_load_plugin_conf_file   , METH_VARARGS },
    { NULL }
};

MOD_INIT
{
    PyObject *m;
    MOD_DEF(m, "_pyabrt", module_doc, module_methods);
    if (m == NULL)
        return MOD_ERROR_VAL;
    return MOD_SUCCESS_VAL(m);
}
