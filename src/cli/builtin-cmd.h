/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#ifndef _BUILTIN_CMD_H_
#define _BUILTIN_CMD_H_

extern int cmd_list(int argc, const char **argv);
extern int cmd_remove(int argc, const char **argv);
extern int _cmd_remove(const char **dirs_strv);
extern int cmd_report(int argc, const char **argv);
extern int _cmd_report(const char **dirs_strv, int remove);
extern int cmd_info(int argc, const char **argv);
extern int _cmd_info(problem_data_t *problem_data, int detailed, int text_size);
extern int cmd_status(int argc, const char **argv);
extern int cmd_process(int argc, const char **argv);

#endif /* _BUILTIN-CMD_H_ */
