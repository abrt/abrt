/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#ifndef PARSE_OPTIONS_H
#define PARSE_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

const char *abrt_init(char **argv);
#define export_abrt_envvars abrt_export_abrt_envvars
void export_abrt_envvars(int pfx);
#define g_progname abrt_g_progname
extern const char *g_progname;


enum parse_opt_type {
    OPTION_BOOL,
    OPTION_GROUP,
    OPTION_STRING,
    OPTION_INTEGER,
    OPTION_OPTSTRING,
    OPTION_LIST,
    OPTION_END,
};

struct options {
    enum parse_opt_type type;
    int short_name;
    const char *long_name;
    void *value;
    const char *argh;
    const char *help;
};

/*
 * s - short_name
 * l - long_name
 * v - value
 * a - option parameter name (for help text)
 * h - help
 */
#define OPT_END()                    { OPTION_END }
#define OPT_GROUP(h)                 { OPTION_GROUP, 0, NULL, NULL, NULL, (h) }
#define OPT_BOOL(     s, l, v,    h) { OPTION_BOOL     , (s), (l), (v), NULL , (h) }
#define OPT_INTEGER(  s, l, v,    h) { OPTION_INTEGER  , (s), (l), (v), "NUM", (h) }
#define OPT_STRING(   s, l, v, a, h) { OPTION_STRING   , (s), (l), (v), (a)  , (h) }
#define OPT_OPTSTRING(s, l, v, a, h) { OPTION_OPTSTRING, (s), (l), (v), (a)  , (h) }
#define OPT_LIST(     s, l, v, a, h) { OPTION_LIST     , (s), (l), (v), (a)  , (h) }

#define OPT__VERBOSE(v)     OPT_BOOL('v', "verbose", (v), _("Be verbose"))

#define parse_opts abrt_parse_opts
unsigned parse_opts(int argc, char **argv, const struct options *opt,
                const char *usage);

#define show_usage_and_die abrt_show_usage_and_die
void show_usage_and_die(const char *usage, const struct options *opt);

#ifdef __cplusplus
}
#endif

#endif
