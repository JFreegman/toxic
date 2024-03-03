/*  python_api.h
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
 *  Copyright (C) 2017-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef PYTHON_API_H
#define PYTHON_API_H

#ifdef PYTHON
#include <Python.h>
#endif /* PYTHON */

#include "toxic.h"

#ifdef PYTHON
PyMODINIT_FUNC PyInit_toxic_api(void);
void terminate_python(void);
void init_python(Toxic *toxic);
void run_python(FILE *fp, char *path);
int do_python_command(int num_args, char (*args)[MAX_STR_SIZE]);
int python_num_registered_handlers(void);
int python_help_max_width(void);
void python_draw_handler_help(WINDOW *win);
#endif

#endif /* PYTHON_API_H */
