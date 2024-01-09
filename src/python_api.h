/*  python_api.h
 *
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
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

#ifndef PYTHON_API_H
#define PYTHON_API_H

#ifdef PYTHON
#include <Python.h>
#endif /* PYTHON */

PyMODINIT_FUNC PyInit_toxic_api(void);
void terminate_python(void);
void init_python(Tox *tox);
void run_python(FILE *fp, char *path);
int do_python_command(int num_args, char (*args)[MAX_STR_SIZE]);
int python_num_registered_handlers(void);
int python_help_max_width(void);
void python_draw_handler_help(WINDOW *win);

#endif /* PYTHON_API_H */
