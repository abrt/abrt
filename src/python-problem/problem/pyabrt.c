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

/* C: void abrt_notify_new_path(const char *path); */
PyObject *p_notify_new_path(PyObject *pself, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
    {
        return NULL;
    }
    abrt_notify_new_path(path);
    Py_RETURN_NONE;
}

static PyObject *
load_settings_to_dict(const char *file, int (*loader)(const char *, GHashTable *))
{
    PyObject *dict = NULL;
    g_autoptr(GHashTable) settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (!loader(file, settings))
    {
        PyErr_SetString(PyExc_OSError, "Failed to load configuration file.");
        goto lacf_error;
    }

    dict = PyDict_New();
    if (dict == NULL)
    {
        goto lacf_error;
    }

    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, settings);
    while(g_hash_table_iter_next(&iter, &key, &value))
    {
        if (0 != PyDict_SetItemString(dict, (char *)key, PyUnicode_FromString((char *)value)))
        {
            goto lacf_error;
        }
    }
    return dict;

lacf_error:
    Py_XDECREF(dict);
    return NULL;
}

/* C: void abrt_load_abrt_conf_file(const char *file, GHashTable *settings); */
PyObject *p_load_conf_file(PyObject *pself, PyObject *args)
{
    const char *file;
    if (!PyArg_ParseTuple(args, "s", &file))
    {
        return NULL;
    }
    return load_settings_to_dict(file, abrt_load_abrt_conf_file);
}

/* C: void abrt_load_abrt_plugin_conf_file(const char *file, GHashTable *settings); */
PyObject *p_load_plugin_conf_file(PyObject *pself, PyObject *args)
{
    const char *file;
    if (!PyArg_ParseTuple(args, "s", &file))
    {
        return NULL;
    }
    return load_settings_to_dict(file, abrt_load_abrt_plugin_conf_file);
}
