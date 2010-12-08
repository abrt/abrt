
#ifndef PARSE_OPTIONS_H
#define PARSE_OPTIONS_H


#ifdef __cplusplus
extern "C" {
#endif

enum parse_opt_type {
    OPTION_BOOL,
    OPTION_GROUP,
    OPTION_STRING,
    OPTION_INTEGER,
    OPTION_OPTSTRING,
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
 * a - argh argument help
 * h - help
 */
#define OPT_END()                    { OPTION_END }
#define OPT_BOOL(s, l, v, h)         { OPTION_BOOL, (s), (l), (v), NULL, (h) }
#define OPT_GROUP(h)                 { OPTION_GROUP, 0, NULL, NULL, NULL, (h) }
#define OPT_INTEGER(s, l, v, h)      { OPTION_INTEGER, (s), (l), (v), "n", (h) }
#define OPT_STRING(s, l, v, a, h)    { OPTION_STRING, (s), (l), (v), (a), (h) }
#define OPT_OPTSTRING(s, l, v, a, h) { OPTION_OPTSTRING, (s), (l), (v), (a), (h) }

#define OPT__VERBOSE(v)     OPT_BOOL('v', "verbose", (v), "be verbose")

#define parse_opts abrt_parse_opts
unsigned parse_opts(int argc, char **argv, const struct options *opt,
                const char *usage);

#define parse_usage_and_die abrt_parse_usage_and_die
void parse_usage_and_die(const char *usage, const struct options *opt);

#ifdef __cplusplus
}
#endif

#endif
