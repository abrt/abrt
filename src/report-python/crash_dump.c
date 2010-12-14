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
#include <structmember.h>

#include <errno.h>
#include "crash_dump.h"
#include "pyreport_common.h"

typedef struct {
    PyObject_HEAD
    crash_data_t *cd;
} p_crash_data;

static void
p_crash_data_dealloc(PyObject *pself)
{
    p_crash_data* self = (p_crash_data*)pself;
    free_crash_data(self->cd);
    self->cd = NULL;
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
p_crash_data_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    p_crash_data *self;

    self = (p_crash_data *)type->tp_alloc(type, 0);
    if (self != NULL)
        self->cd = new_crash_data();

    return (PyObject *)self;
}

static int
p_crash_data_init(PyObject *pself, PyObject *args, PyObject *kwds)
{
    return 0;
}

/*
void add_to_crash_data_ext(crash_data_t *crash_data,
                const char *name,
                const char *content,
                unsigned flags);
*/

static PyObject *p_crash_data_add_ext(PyObject *pself, PyObject *args, PyObject *kwds)
{
    p_crash_data *self = (p_crash_data*)pself;

    const char *name;
    const char *content;
    int FLAGS;
    if(!PyArg_ParseTuple(args, "ssi", &name, &content, &FLAGS))
    {
        PyErr_SetString(ReportError, strerror(errno));
        return NULL;
    }
    add_to_crash_data_ext(self->cd, name, content, FLAGS);

    /* every function returns PyObject to return void we need to do this */
    Py_RETURN_NONE;
}
static PyObject *p_crash_data_add(PyObject *pself, PyObject *args, PyObject *kwds)
{
    p_crash_data *self = (p_crash_data*)pself;

    const char *name;
    const char *content;
    if(!PyArg_ParseTuple(args, "ss", &name, &content))
    {
        /* PyArg_ParseTuple raises the exception saying why it fails
         * eg: TypeError: function takes exactly 2 arguments (1 given)
         */
        return NULL;
    }
    add_to_crash_data(self->cd, name, content);

    /* every function returns PyObject to return void we need to do this */
    Py_RETURN_NONE;
}

/*
static inline struct crash_item *get_crash_data_item_or_NULL(crash_data_t *crash_data, const char *key)
{
    return (struct crash_item *)g_hash_table_lookup(crash_data, key);
}
*/

static PyObject *p_get_crash_data_item(PyObject *pself, PyObject *args, PyObject *kwds)
{
    p_crash_data *self = (p_crash_data*)pself;

    const char *key;
    if(!PyArg_ParseTuple(args, "s", &key))
    {
        return NULL;
    }
    struct crash_item *ci = get_crash_data_item_or_NULL(self->cd, key);
    return Py_BuildValue("sI", ci->content, ci->flags);
}


static PyObject *p_create_crash_dump_dir(PyObject *pself, PyObject *args)
{
    p_crash_data *self = (p_crash_data*)pself;
    struct dump_dir *dd = create_crash_dump_dir(self->cd);
    if(dd == NULL)
    {
        PyErr_SetString(ReportError, "Can't create the dump dir");
        return NULL;
    }
    //FIXME: return a python representation of dump_dir, when we have it..
    Py_RETURN_NONE;
}

static PyMemberDef p_crash_data_members[] = {
    {NULL}  /* Sentinel */
};

static PyMethodDef p_crash_data_methods[] = {
    {"add", (PyCFunction)p_crash_data_add, METH_VARARGS,
        "Adds item to the crash data using default flags"
    },
    {"add_ext", (PyCFunction)p_crash_data_add_ext, METH_VARARGS,
        "Adds item to the crash data"
    },
    {"get", (PyCFunction)p_get_crash_data_item, METH_VARARGS,
        "Gets the value of item indexed by the key"
    },
    {"to_dump_dir", (PyCFunction)p_create_crash_dump_dir, METH_NOARGS,
        "Saves the crash_data to"LOCALSTATEDIR"/run/abrt/tmp-<pid>-<time>"
    },
    {NULL}  /* Sentinel */
};

PyTypeObject p_crash_data_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "report.crash_data",             /*tp_name*/
    sizeof(p_crash_data),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    p_crash_data_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "crash_data objects",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    p_crash_data_methods,             /* tp_methods */
    p_crash_data_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    p_crash_data_init,      /* tp_init */
    0,                         /* tp_alloc */
    p_crash_data_new,                 /* tp_new */
};

PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};
