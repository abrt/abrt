/*
    On-disk storage of problem data

    Copyright (C) 2010  Abrt team
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
#include "common.h"

/*** init/cleanup ***/

static PyObject *
p_dump_dir_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    p_dump_dir *self = (p_dump_dir *)type->tp_alloc(type, 0);
    if (self)
        self->dd = NULL;
    return (PyObject *)self;
}

static void
p_dump_dir_dealloc(PyObject *pself)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    dd_close(self->dd);
    self->dd = NULL;
    self->ob_type->tp_free(pself);
}

//static int
//p_dump_dir_init(PyObject *pself, PyObject *args, PyObject *kwds)
//{
//    return 0;
//}


/*** methods ***/

/* void dd_close(struct dump_dir *dd); */
static PyObject *p_dd_close(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    dd_close(self->dd);
    self->dd = NULL;
    Py_RETURN_NONE;
}

/* void dd_delete(struct dump_dir *dd); */
static PyObject *p_dd_delete(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
//Do we want to disallow delete() on non-opened dd?
//    if (!self->dd)
//    {
//        PyErr_SetString(ReportError, "dump dir is not open");
//        return NULL;
//    }
    dd_delete(self->dd);
    self->dd = NULL;
    Py_RETURN_NONE;
}

/* int dd_exist(struct dump_dir *dd, const char *path); */
static PyObject *p_dd_exist(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    if (!self->dd)
    {
        PyErr_SetString(ReportError, "dump dir is not open");
        return NULL;
    }
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
    {
        return NULL;
    }
    return Py_BuildValue("i", dd_exist(self->dd, path));
}

/* DIR *dd_init_next_file(struct dump_dir *dd); */
//static PyObject *p_dd_init_next_file(PyObject *pself, PyObject *args);
/* int dd_get_next_file(struct dump_dir *dd, char **short_name, char **full_name); */
//static PyObject *p_dd_get_next_file(PyObject *pself, PyObject *args);

/* char* dd_load_text_ext(const struct dump_dir *dd, const char *name, unsigned flags); */
/* char* dd_load_text(const struct dump_dir *dd, const char *name); */
static PyObject *p_dd_load_text(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    if (!self->dd)
    {
        PyErr_SetString(ReportError, "dump dir is not open");
        return NULL;
    }
    const char *name;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "s|i", &name, &flags))
    {
        return NULL;
    }
    char *val = dd_load_text_ext(self->dd, name, flags);
    PyObject *obj = Py_BuildValue("s", val); /* NB: if val is NULL, obj is None */
    free(val);
    return obj;
}

/* void dd_save_text(struct dump_dir *dd, const char *name, const char *data); */
static PyObject *p_dd_save_text(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    if (!self->dd)
    {
        PyErr_SetString(ReportError, "dump dir is not open");
        return NULL;
    }
    const char *name;
    const char *data;
    if (!PyArg_ParseTuple(args, "ss", &name, &data))
    {
        return NULL;
    }
    dd_save_text(self->dd, name, data);
    Py_RETURN_NONE;
}

/* void dd_save_binary(struct dump_dir *dd, const char *name, const char *data, unsigned size); */
static PyObject *p_dd_save_binary(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    if (!self->dd)
    {
        PyErr_SetString(ReportError, "dump dir is not open");
        return NULL;
    }
    const char *name;
    const char *data;
    unsigned size;
    if (!PyArg_ParseTuple(args, "ssI", &name, &data, &size))
    {
        return NULL;
    }
    dd_save_binary(self->dd, name, data, size);
    Py_RETURN_NONE;
}


/*** attribute getters/setters ***/

static PyObject *get_name(PyObject *pself, void *unused)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    if (self->dd)
        return Py_BuildValue("s", self->dd->dd_dirname);
    Py_RETURN_NONE;
}

//static PyObject *set_name(PyObject *pself, void *unused)
//{
//    PyErr_SetString(ReportError, "dump dir name is not settable");
//    Py_RETURN_NONE;
//}


/*** type object ***/

static PyMethodDef p_dump_dir_methods[] = {
    /* method_name, func, flags, doc_string */
    { "close"      , p_dd_close, METH_NOARGS, NULL },
    { "delete"     , p_dd_delete, METH_NOARGS, NULL },
    { "exist"      , p_dd_exist, METH_VARARGS, NULL },
    { "load_text"  , p_dd_load_text, METH_VARARGS, NULL },
    { "save_text"  , p_dd_save_text, METH_VARARGS, NULL },
    { "save_binary", p_dd_save_binary, METH_VARARGS, NULL },
    { NULL }
};

static PyGetSetDef p_dump_dir_getset[] = {
    /* attr_name, getter_func, setter_func, doc_string, void_param */
    { (char*) "name", get_name, NULL /*set_name*/ },
    { NULL }
};

/* Support for "dd = dd_opendir(...); if [not] dd: ..." */
static int p_dd_is_non_null(PyObject *pself)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    return self->dd != NULL;
}
static PyNumberMethods p_dump_dir_number_methods = {
    .nb_nonzero = p_dd_is_non_null,
};

PyTypeObject p_dump_dir_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name      = "report.dump_dir",
    .tp_basicsize = sizeof(p_dump_dir),
    /* Py_TPFLAGS_BASETYPE means "can be subtyped": */
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new       = p_dump_dir_new,
    .tp_dealloc   = p_dump_dir_dealloc,
    //.tp_init      = p_dump_dir_init,
    //.tp_members   = p_dump_dir_members,
    .tp_methods   = p_dump_dir_methods,
    .tp_as_number = &p_dump_dir_number_methods,
    .tp_getset    = p_dump_dir_getset,
};


/*** module-level functions ***/

/* struct dump_dir *dd_opendir(const char *dir, int flags); */
PyObject *p_dd_opendir(PyObject *module, PyObject *args)
{
    const char *dir;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "s|i", &dir, &flags))
        return NULL;
    p_dump_dir *new_dd = PyObject_New(p_dump_dir, &p_dump_dir_type);
    if (!new_dd)
        return NULL;
    new_dd->dd = dd_opendir(dir, flags);
    return (PyObject*)new_dd;
}

/* struct dump_dir *dd_create(const char *dir, uid_t uid); */
PyObject *p_dd_create(PyObject *module, PyObject *args)
{
    const char *dir;
    int uid = -1;
    if (!PyArg_ParseTuple(args, "s|i", &dir, &uid))
        return NULL;
    p_dump_dir *new_dd = PyObject_New(p_dump_dir, &p_dump_dir_type);
    if (!new_dd)
        return NULL;
    new_dd->dd = dd_create(dir, uid, 0640);
    return (PyObject*)new_dd;
}

/* void delete_dump_dir(const char *dirname); */
PyObject *p_delete_dump_dir(PyObject *pself, PyObject *args)
{
    const char *dirname;
    if (!PyArg_ParseTuple(args, "s", &dirname))
        return NULL;
    delete_dump_dir(dirname);
    Py_RETURN_NONE;
}
