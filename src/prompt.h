/*  prompt.h
 *
 *
 *  Copyright (C) 2014 Toxic All Rights Reserved.
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

#ifndef PROMPT_H_UZYGWFFL
#define PROMPT_H_UZYGWFFL

#define X_OFST 2    /* offset to account for prompt char */


#ifdef _SUPPORT_AUDIO 
    #define AC_NUM_GLOB_COMMANDS 17
#else 
    #define AC_NUM_GLOB_COMMANDS 15
#endif /* _SUPPORT_AUDIO */ 

ToxWindow new_prompt(void);
void prep_prompt_win(void);
void prompt_init_statusbar(ToxWindow *self, Tox *m);
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len);
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len);
void prompt_update_status(ToxWindow *prompt, uint8_t status);
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected);

#endif /* end of include guard: PROMPT_H_UZYGWFFL */
