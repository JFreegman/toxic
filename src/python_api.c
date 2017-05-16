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

static PyObject *python_api_display(PyObject *self, PyObject *args)
{
    const char *msg;
    if (!PyArg_ParseTuple(args, "s", &msg))
        return NULL;

    api_display(msg);

    Py_RETURN_NONE;
}

static PyMethodDef ToxicApiMethods[] = {
    {"display", python_api_display, METH_VARARGS, "Display a message to the primary prompt"},
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

void init_python(void)
{
    PyImport_AppendInittab("toxic_api", PyInit_toxic_api);
    /* TODO: Set Python program name. */
    Py_Initialize();
}

void run_python(FILE *fp, char *path)
{
    PyRun_SimpleFile(fp, path);
}
