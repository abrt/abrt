/*
    On-disk storage of crash dumps

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
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
#include <structmember.h>

#include <errno.h>
#include "crash_dump.h"
#include "dump_dir.h"
#include "common.h"

static void
p_dump_dir_dealloc(PyObject *pself)
{
    p_dump_dir *self = (p_dump_dir*)pself;
    dd_close(self->dd);
    self->dd = NULL;
    self->ob_type->tp_free(pself);
}

static PyObject *
p_dump_dir_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    p_dump_dir *self = (p_dump_dir *)type->tp_alloc(type, 0);
    if (self)
        self->dd = NULL;
    return (PyObject *)self;
}

//static int
//p_dump_dir_init(PyObject *pself, PyObject *args, PyObject *kwds)
//{
//    return 0;
//}


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
    dd_delete(self->dd);
    self->dd = NULL;
    Py_RETURN_NONE;
}

/* int dd_exist(struct dump_dir *dd, const char *path); */
static PyObject *p_dd_exist(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
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
static PyObject *p_dd_load_text_ext(PyObject *pself, PyObject *args)
{
    p_dump_dir *self = (p_dump_dir*)pself;
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

static PyMethodDef p_dump_dir_methods[] = {
    { "close"      , p_dd_close, METH_NOARGS, NULL },
    { "delete"     , p_dd_delete, METH_NOARGS, NULL },
    { "exist"      , p_dd_exist, METH_VARARGS, NULL },
    { "load_text"  , p_dd_load_text_ext, METH_VARARGS, NULL },
    { "save_text"  , p_dd_save_text, METH_VARARGS, NULL },
    { "save_binary", p_dd_save_binary, METH_VARARGS, NULL },
    { NULL }
};

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
    .tp_dealloc   = p_dump_dir_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "dump_dir objects",
    .tp_methods   = p_dump_dir_methods,
    //.tp_members   = p_dump_dir_members,
    //.tp_init      = p_dump_dir_init,
    .tp_new       = p_dump_dir_new,
    .tp_as_number = &p_dump_dir_number_methods,
};


/* struct dump_dir *dd_opendir(const char *dir, int flags); */
PyObject *p_dd_opendir(PyObject *module, PyObject *args)
{
    const char *dir;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "s|i", &dir, &flags))
        return NULL;
//    PyObject *new_obj = PyObject_CallObject(&p_dump_dir_type, NULL); /* constructor call */
//    p_dump_dir *new_dd = (p_dump_dir*)new_obj;
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
    int uid;
    if (!PyArg_ParseTuple(args, "si", &dir, &uid))
        return NULL;
//    PyObject *new_obj = PyObject_CallObject(&p_dump_dir_type, NULL); /* constructor call */
//    p_dump_dir *new_dd = (p_dump_dir*)new_obj;
    p_dump_dir *new_dd = PyObject_New(p_dump_dir, &p_dump_dir_type);
    if (!new_dd)
        return NULL;
    new_dd->dd = dd_create(dir, uid);
    return (PyObject*)new_dd;
}

/* void delete_crash_dump_dir(const char *dd_dir); */
//static PyObject *p_delete_crash_dump_dir(PyObject *pself, PyObject *args);
