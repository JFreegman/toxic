/*  python_api.c
 *
 *
 *  Copyright (C) 2017 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic.
 *
 *  Toxic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Toxic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Toxic.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Python.h>

#include "api.h"

extern Tox *user_tox;

static PyObject *python_api_display(PyObject *self, PyObject *args)
{
    const char *msg;
    if (!PyArg_ParseTuple(args, "s", &msg))
        return NULL;
    api_display(msg);
    return Py_None;
}

static PyObject *python_api_get_nick(PyObject *self, PyObject *args)
{
    char     *name;
    PyObject *ret;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    name = api_get_nick();
    if (name == NULL)
        return NULL;
    ret  = Py_BuildValue("s", name);
    free(name);
    return ret;
}

static PyObject *python_api_get_status(PyObject *self, PyObject *args)
{
    TOX_USER_STATUS  status;
    PyObject        *ret;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    status = api_get_status();
    ret    = Py_BuildValue("i", status);
    return ret;
}

static PyObject *python_api_get_status_message(PyObject *self, PyObject *args)
{
    char     *status;
    PyObject *ret;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    status = api_get_status_message();
    if (status == NULL)
        return NULL;
    ret    = Py_BuildValue("s", status);
    free(status);
    return ret;
}

static PyObject *python_api_execute(PyObject *self, PyObject *args)
{
    int         mode;
    const char *command;
    if (!PyArg_ParseTuple(args, "si", &command, &mode))
        return NULL;
    api_execute(command, mode);
    return Py_None;
}

static PyMethodDef ToxicApiMethods[] = {
    {"display", python_api_display, METH_VARARGS, "Display a message to the primary prompt"},
    {"get_nick", python_api_get_nick, METH_VARARGS, "Return the user's current nickname"},
    {"get_status_message", python_api_get_status_message, METH_VARARGS, "Return the user's current status message"},
    {"execute", python_api_execute, METH_VARARGS, "Execute a command like `/nick`"},
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef toxic_api_module = {
    PyModuleDef_HEAD_INIT,
    "toxic_api",
    NULL, /* TODO: Module documentation. */
    -1,   /* TODO: Assumption that no per-interpreter state is maintained. */
    ToxicApiMethods
};

PyMODINIT_FUNC PyInit_toxic_api(void)
{
    return PyModule_Create(&toxic_api_module);
}

void terminate_python(void)
{
    Py_FinalizeEx();
}

void init_python(Tox *m)
{
    wchar_t *program = Py_DecodeLocale("toxic", NULL);
    user_tox = m;
    PyImport_AppendInittab("toxic_api", PyInit_toxic_api);
    Py_SetProgramName(program);
    Py_Initialize();
    PyMem_RawFree(program);
}

void run_python(FILE *fp, char *path)
{
    PyRun_SimpleFile(fp, path);
}
