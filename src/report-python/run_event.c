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
#include "common.h"
#include "problem_data.h"
#include "run_event.h"

typedef struct {
    PyObject_HEAD
    struct run_event_state *state;
    PyObject *post_run_callback;
    PyObject *logging_callback;
} p_run_event_state;


/*** init/cleanup ***/

static int post_run_callback(const char *dump_dir_name, void *param);
static char *logging_callback(char *log_line, void *param);

static PyObject *p_run_event_state_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    p_run_event_state *self = (p_run_event_state *)type->tp_alloc(type, 0);
    if (self)
    {
        self->state = new_run_event_state();
        self->state->post_run_callback = post_run_callback;
        self->state->logging_callback = logging_callback;
        self->state->post_run_param = self;
        self->state->logging_param = self;
    }
    return (PyObject *)self;
}

static void p_run_event_state_dealloc(PyObject *pself)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    free_run_event_state(self->state);
    self->state = NULL;
    Py_XDECREF(self->post_run_callback);
    self->post_run_callback = NULL;
    Py_XDECREF(self->logging_callback);
    self->logging_callback = NULL;
    self->ob_type->tp_free(pself);
}

//static int
//p_run_event_state_init(PyObject *pself, PyObject *args, PyObject *kwds)
//{
//    return 0;
//}


/*** methods ***/

/* First, C-level callback helpers for run_event_on_FOO(): */
static int post_run_callback(const char *dump_dir_name, void *param)
{
    PyObject *obj = (PyObject*)param;
    PyObject *ret = PyObject_CallMethod(obj, (char*) "post_run_callback", (char*) "(s)", dump_dir_name);
    int r = 0;
    if (ret)
    {
        r = PyInt_AsLong(ret);
        Py_DECREF(ret);
    }
    // TODO: handle exceptions: if (PyErr_Occurred()) ...
    return r;
}
static char *logging_callback(char *log_line, void *param)
{
    PyObject *obj = (PyObject*)param;
    PyObject *ret = PyObject_CallMethod(obj, (char*) "logging_callback", (char*) "(s)", log_line);
    Py_XDECREF(ret);
    // TODO: handle exceptions: if (PyErr_Occurred()) ...
    return log_line; /* signaling to caller that we didnt consume the string */
}

/* int run_event_on_dir_name(struct run_event_state *state, const char *dump_dir_name, const char *event); */
static PyObject *p_run_event_on_dir_name(PyObject *pself, PyObject *args)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    const char *dump_dir_name;
    const char *event;
    if (!PyArg_ParseTuple(args, "ss", &dump_dir_name, &event))
    {
        return NULL;
    }
    int r = run_event_on_dir_name(self->state, dump_dir_name, event);
    PyObject *obj = Py_BuildValue("i", r);
    return obj;
}

/* int run_event_on_problem_data(struct run_event_state *state, problem_data_t *data, const char *event); */
static PyObject *p_run_event_on_problem_data(PyObject *pself, PyObject *args)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    p_problem_data *cd;
    const char *event;
    if (!PyArg_ParseTuple(args, "O!s", &p_problem_data_type, &cd, &event))
    {
        return NULL;
    }
    int r = run_event_on_problem_data(self->state, cd->cd, event);
    PyObject *obj = Py_BuildValue("i", r);
    return obj;
}

/* TODO: char *list_possible_events(struct dump_dir *dd, const char *dump_dir_name, const char *pfx); */


/*** attribute getters/setters ***/

static PyObject *get_post_run_callback(PyObject *pself, void *unused)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    if (self->post_run_callback)
    {
        Py_INCREF(self->post_run_callback);
        return self->post_run_callback;
    }
    Py_RETURN_NONE;
}

static PyObject *get_logging_callback(PyObject *pself, void *unused)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    if (self->logging_callback)
    {
        Py_INCREF(self->logging_callback);
        return self->logging_callback;
    }
    Py_RETURN_NONE;
}

static int set_post_run_callback(PyObject *pself, PyObject *callback, void *unused)
{
    p_run_event_state *self = (p_run_event_state*)pself;
//WRONG: we aren't a Python function, calling convention is different
//    PyObject *callback;
//    if (!PyArg_ParseTuple(args, "O", &callback))
//        return -1;
    if (!PyCallable_Check(callback))
    {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return -1;
    }
    Py_INCREF(callback);
    Py_XDECREF(self->post_run_callback);
    self->post_run_callback = callback;
    return 0;
}

static int set_logging_callback(PyObject *pself, PyObject *callback, void *unused)
{
    p_run_event_state *self = (p_run_event_state*)pself;
    if (!PyCallable_Check(callback))
    {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return -1;
    }
    Py_INCREF(callback);
    Py_XDECREF(self->logging_callback);
    self->logging_callback = callback;
    return 0;
}


/*** type object ***/

//static PyMemberDef p_run_event_state_members[] = {
//    { NULL }
//};

static PyMethodDef p_run_event_state_methods[] = {
    /* method_name, func, flags, doc_string */
    { "run_event_on_dir_name"  , p_run_event_on_dir_name  , METH_VARARGS },
    { "run_event_on_problem_data", p_run_event_on_problem_data, METH_VARARGS },
    { NULL }
};

static PyGetSetDef p_run_event_state_getset[] = {
    /* attr_name, getter_func, setter_func, doc_string, void_param */
    { (char*) "post_run_callback", get_post_run_callback, set_post_run_callback },
    { (char*) "logging_callback" , get_logging_callback , set_logging_callback  },
    { NULL }
};

PyTypeObject p_run_event_state_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name      = "report.run_event_state",
    .tp_basicsize = sizeof(p_run_event_state),
    /* Py_TPFLAGS_BASETYPE means "can be subtyped": */
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new       = p_run_event_state_new,
    .tp_dealloc   = p_run_event_state_dealloc,
    //.tp_init      = p_run_event_state_init,
    //.tp_members   = p_run_event_state_members,
    .tp_methods   = p_run_event_state_methods,
    .tp_getset    = p_run_event_state_getset,
};
