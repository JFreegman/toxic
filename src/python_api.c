/*  python_api.c
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
 *  Copyright (C) 2017-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include "python_api.h"

#include "api.h"
#include "toxic.h"
#include "execute.h"

#ifdef PYTHON
#include <Python.h>

extern Toxic       *user_toxic;

static struct python_registered_func {
    char     *name;
    char     *help;
    PyObject *callback;
    struct python_registered_func *next;
} python_commands = {0};

static PyObject *python_api_display(PyObject *self, PyObject *args)
{
    const char *msg;

    if (!PyArg_ParseTuple(args, "s", &msg)) {
        return NULL;
    }

    api_display(msg);
    return Py_None;
}

static PyObject *python_api_get_nick(PyObject *self, PyObject *args)
{
    char     *name;
    PyObject *ret;

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    name = api_get_nick();

    if (name == NULL) {
        return NULL;
    }

    ret  = Py_BuildValue("s", name);
    free(name);
    return ret;
}

static PyObject *python_api_get_status(PyObject *self, PyObject *args)
{
    PyObject        *ret = NULL;

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    switch (api_get_status()) {
        case TOX_USER_STATUS_NONE:
            ret = Py_BuildValue("s", "online");
            break;

        case TOX_USER_STATUS_AWAY:
            ret = Py_BuildValue("s", "away");
            break;

        case TOX_USER_STATUS_BUSY:
            ret = Py_BuildValue("s", "busy");
            break;
    }

    return ret;
}

static PyObject *python_api_get_status_message(PyObject *self, PyObject *args)
{
    char     *status;
    PyObject *ret;

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    status = api_get_status_message();

    if (status == NULL) {
        return NULL;
    }

    ret    = Py_BuildValue("s", status);
    free(status);
    return ret;
}

static PyObject *python_api_get_all_friends(PyObject *self, PyObject *args)
{
    FriendsList  friends;
    char pubkey_buf[TOX_PUBLIC_KEY_SIZE * 2 + 1];

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    friends = api_get_friendslist();
    PyObject *ret = PyList_New(0);

    for (size_t i = 0; i < friends.num_friends; i++) {
        for (size_t ii = 0; ii < TOX_PUBLIC_KEY_SIZE; ii++) {
            snprintf(pubkey_buf + ii * 2, 3, "%02X", friends.list[i].pub_key[ii] & 0xff);
        }

        pubkey_buf[TOX_PUBLIC_KEY_SIZE * 2] = '\0';
        PyObject *cur = Py_BuildValue("(s,s)", friends.list[i].name, pubkey_buf);
        PyList_Append(ret, cur);
    }

    return ret;
}

static PyObject *python_api_send(PyObject *self, PyObject *args)
{
    const char *msg;

    if (!PyArg_ParseTuple(args, "s", &msg)) {
        return NULL;
    }

    api_send(msg);
    return Py_None;
}

static PyObject *python_api_execute(PyObject *self, PyObject *args)
{
    int         mode;
    const char *command;

    if (!PyArg_ParseTuple(args, "si", &command, &mode)) {
        return NULL;
    }

    api_execute(command, mode);
    return Py_None;
}

static PyObject *python_api_register(PyObject *self, PyObject *args)
{
    struct python_registered_func *cur;
    size_t      command_len, help_len;
    const char *command, *help;
    PyObject   *callback;

    if (!PyArg_ParseTuple(args, "ssO:register_command", &command, &help, &callback)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Calback parameter must be callable");
        return NULL;
    }

    if (command[0] != '/') {
        PyErr_SetString(PyExc_TypeError, "Command must be prefixed with a '/'");
        return NULL;
    }

    for (cur = &python_commands; ; cur = cur->next) {
        if (cur->name != NULL && !strcmp(command, cur->name)) {
            Py_XDECREF(cur->callback);
            Py_XINCREF(callback);
            cur->callback = callback;
            break;
        }

        if (cur->next == NULL) {
            Py_XINCREF(callback);
            cur->next = malloc(sizeof(struct python_registered_func));

            if (cur->next == NULL) {
                return PyErr_NoMemory();
            }

            command_len     = strlen(command);
            cur->next->name = malloc(command_len + 1);

            if (cur->next->name == NULL) {
                return PyErr_NoMemory();
            }

            strncpy(cur->next->name, command, command_len + 1);
            help_len        = strlen(help);
            cur->next->help = malloc(help_len + 1);

            if (cur->next->help == NULL) {
                return PyErr_NoMemory();
            }

            strncpy(cur->next->help, help, help_len + 1);
            cur->next->callback = callback;
            cur->next->next     = NULL;
            break;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef ToxicApiMethods[] = {
    {"display",            python_api_display,            METH_VARARGS, "Display a message to the current prompt"},
    {"get_nick",           python_api_get_nick,           METH_VARARGS, "Return the user's current nickname"},
    {"get_status",         python_api_get_status,         METH_VARARGS, "Returns the user's current status"},
    {"get_status_message", python_api_get_status_message, METH_VARARGS, "Return the user's current status message"},
    {"get_all_friends",    python_api_get_all_friends,    METH_VARARGS, "Return all of the user's friends"},
    {"send",               python_api_send,               METH_VARARGS, "Send the message to the current user"},
    {"execute",            python_api_execute,            METH_VARARGS, "Execute a command like `/nick`"},
    {"register",           python_api_register,           METH_VARARGS, "Register a command like `/nick` to a Python function"},
    {NULL,                 NULL,                          0,            NULL},
};

static struct PyModuleDef toxic_api_module = {
    PyModuleDef_HEAD_INIT,
    "toxic_api",
    NULL,
    -1,
    ToxicApiMethods
};

PyMODINIT_FUNC PyInit_toxic_api(void)
{
    PyObject *m = PyModule_Create(&toxic_api_module);
    PyObject *global_command_const    = Py_BuildValue("i", GLOBAL_COMMAND_MODE);
    PyObject *chat_command_const      = Py_BuildValue("i", CHAT_COMMAND_MODE);
    PyObject *conference_command_const = Py_BuildValue("i", CONFERENCE_COMMAND_MODE);
    PyObject_SetAttrString(m, "GLOBAL_COMMAND",    global_command_const);
    PyObject_SetAttrString(m, "CHAT_COMMAND",      chat_command_const);
    PyObject_SetAttrString(m, "CONFERENCE_COMMAND", conference_command_const);
    Py_DECREF(global_command_const);
    Py_DECREF(chat_command_const);
    Py_DECREF(conference_command_const);
    return m;
}

void terminate_python(void)
{
    if (python_commands.name != NULL) {
        free(python_commands.name);
    }

    struct python_registered_func *cur = NULL;

    for (cur = python_commands.next; cur != NULL;) {
        struct python_registered_func *old = cur;
        cur = cur->next;
        free(old->name);
        free(old);
    }

    Py_Finalize();
}

void init_python(Toxic *toxic)
{
    user_toxic = toxic;
    PyImport_AppendInittab("toxic_api", PyInit_toxic_api);
    Py_Initialize();
}

void run_python(FILE *fp, char *path)
{
    PyRun_SimpleFile(fp, path);
}

int do_python_command(int num_args, char (*args)[MAX_STR_SIZE])
{
    int i;
    PyObject *callback_args, *args_strings;
    struct python_registered_func *cur;

    for (cur = &python_commands; cur != NULL; cur = cur->next) {
        if (cur->name == NULL) {
            continue;
        }

        if (!strcmp(args[0], cur->name)) {
            args_strings = PyList_New(0);

            for (i = 1; i < num_args; i++) {
                PyList_Append(args_strings, Py_BuildValue("s", args[i]));
            }

            callback_args = PyTuple_Pack(1, args_strings);

            if (PyObject_CallObject(cur->callback, callback_args) == NULL) {
                api_display("Exception raised in callback function");
            }

            return 0;
        }
    }

    return 1;
}

int python_num_registered_handlers(void)
{
    int n = 0;
    struct python_registered_func *cur;

    for (cur = &python_commands; cur != NULL; cur = cur->next) {
        if (cur->name != NULL) {
            n++;
        }
    }

    return n;
}

int python_help_max_width(void)
{
    size_t tmp;
    int    max = 0;
    struct python_registered_func *cur;

    for (cur = &python_commands; cur != NULL; cur = cur->next) {
        if (cur->name != NULL) {
            tmp = strlen(cur->help);
            max = tmp > max ? tmp : max;
        }
    }

    max = max > 50 ? 50 : max;
    return 37 + max;
}

void python_draw_handler_help(WINDOW *win)
{
    struct python_registered_func *cur;

    for (cur = &python_commands; cur != NULL; cur = cur->next) {
        if (cur->name != NULL) {
            wprintw(win, "  %-29s: %.50s\n", cur->name, cur->help);
        }
    }
}

#endif /* PYTHON */
