/* -*-mode:c++;c-file-style:"bsd";c-basic-offset:4;indent-tabs-mode:nil-*-
    main.cpp - parses command line arguments

    Copyright (C) 2009  RedHat inc.

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
#include <argp.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include "config.h"
#include "backtrace.h"
#include "strbuf.h"

/* Too large files are trimmed. */
#define FILE_SIZE_LIMIT 20000000 /* ~ 20 MB */

#define EX_PARSINGFAILED EX__MAX + 1  /* = 79 */
#define EX_THREADDETECTIONFAILED EX__MAX + 2 /* = 80 */

const char *argp_program_version = "abrt-backtrace " VERSION;
const char *argp_program_bug_address = "<crash-catcher@lists.fedorahosted.org>";

static char doc[] = "abrt-backtrace -- backtrace analyzer";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE";

static struct argp_option options[] = {
    {"independent"           , 'i', 0  , 0, "Prints independent backtrace (fallback)"},
    {"single-thread"         , 'n', 0  , 0, "Display the crash thread only in the backtrace"},
    {"frame-depth"           , 'd', "N", 0, "Display only top N frames under the crash frame"},
    {"remove-exit-handlers"  , 'r', 0  , 0, "Removes exit handler frames from the displayed backtrace"},
    {"remove-noncrash-frames", 'm', 0  , 0, "Removes common frames known as not causing crash"},
    {"rate"                  , 'a', 0  , 0, "Prints the backtrace rating from 0 to 4"},
    {"debug-parser"          , 'p', 0  , 0, "Prints parser debug information"},
    {"debug-scanner"         , 's', 0  , 0, "Prints scanner debug information"},
    {"verbose"               , 'v', 0  , 0, "Print human-friendly superfluous output."},
  { 0 }
};

struct arguments
{
    bool independent;
    bool single_thread;
    int frame_depth; /* negative == do not limit the depth */
    bool remove_exit_handlers;
    bool remove_noncrash_frames;
    bool debug_parser;
    bool debug_scanner;
    bool verbose;
    bool rate;
    char *filename;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments *arguments = (struct arguments*)state->input;

    switch (key)
    {
    case 'i': arguments->independent = true; break;
    case 'n': arguments->single_thread = true; break;
    case 'd':
        if (1 != sscanf(arg, "%d", &arguments->frame_depth))
        {
            /* Must be a number. */
            argp_usage(state);
            exit(EX_USAGE); /* Invalid argument */
        }
        break;
    case 'r': arguments->remove_exit_handlers = true; break;
    case 'm': arguments->remove_noncrash_frames = true; break;
    case 'p': arguments->debug_parser = true; break;
    case 's': arguments->debug_scanner = true; break;
    case 'v': arguments->verbose = true; break;
    case 'a': arguments->rate = true; break;

    case ARGP_KEY_ARG:
        if (arguments->filename)
        {
            /* Too many arguments. */
            argp_usage(state);
            exit(EX_USAGE); /* Invalid argument */
        }
        arguments->filename = arg;
        break;

    case ARGP_KEY_END:
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

#define PYTHON_BACKTRACE_ID1 "\n\nTraceback (most recent call last):\n"
#define PYTHON_BACKTRACE_ID2 "\n\nLocal variables in innermost frame:\n"

int main(int argc, char **argv)
{
    /* Set options default values and parse program command line. */
    struct arguments arguments;
    arguments.independent = false;
    arguments.frame_depth = -1;
    arguments.single_thread = false;
    arguments.remove_exit_handlers = false;
    arguments.remove_noncrash_frames = false;
    arguments.debug_parser = false;
    arguments.debug_scanner = false;
    arguments.verbose = false;
    arguments.filename = 0;
    arguments.rate = false;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    /* If we are about to rate a backtrace, the other values must be set accordingly,
       no matter what the user set on the command line. */
    if (arguments.rate)
    {
        arguments.independent = false;
        arguments.frame_depth = 5;
        arguments.single_thread = true;
        arguments.remove_exit_handlers = true;
        arguments.remove_noncrash_frames = true;
    }

    char *bttext = NULL;

    /* If a filename was provided, read input from file.
       Otherwise read from stdin. */
    if (arguments.filename)
    {
        /* Open input file, and parse it. */
        FILE *fp = fopen(arguments.filename, "r");
        if (!fp)
        {
            fprintf(stderr, "Unable to open '%s'.\n", arguments.filename);
            exit(EX_NOINPUT); /* No such file or directory */
        }

        /* Header and footer of the backtrace is stripped to simplify the parser.
         * A drawback is that the backtrace must be loaded to memory.
         */
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (size > FILE_SIZE_LIMIT)
        {
            fprintf(stderr, "Input file too big (%zd). Maximum size is %d.\n",
                    size, FILE_SIZE_LIMIT);
            exit(EX_IOERR);
        }

        /* Handle the case that the input file is empty.
         * The code is not designed to support completely empty backtrace.
         * Silently exit indicating success.
         */
        if (size == 0)
        {
            fclose(fp);
            exit(0);
        }

        bttext = malloc(size + 1);
        if (!bttext)
        {
            fclose(fp);
            fputs("malloc failed", stderr);
            exit(EX_OSERR);
        }

        if (1 != fread(bttext, size, 1, fp))
        {
            fclose(fp);
            fprintf(stderr, "Unable to read from '%s'.\n", arguments.filename);
            exit(EX_IOERR); /* IO Error */
        }

        bttext[size] = '\0';
        fclose(fp);
    }
    else
    {
        struct strbuf *btin = strbuf_new();
        int c;
        while ((c = getchar()) != EOF && c != '\0')
            strbuf_append_char(btin, (char)c);

        strbuf_append_char(btin, '\0');
        bttext = btin->buf;
        strbuf_free_nobuf(btin); /* free btin, but not its internal buffer */
    }

    /* Detect Python backtraces. If it is a Python backtrace,
     * silently exit for now.
     */
    if (strstr(bttext, PYTHON_BACKTRACE_ID1) != NULL
        && strstr(bttext, PYTHON_BACKTRACE_ID2) != NULL)
    {
        if (arguments.rate)
            puts("4");
        exit(0);
    }

    /* Print independent backtrace and exit. */
    if (arguments.independent)
    {
        struct strbuf *ibt = independent_backtrace(bttext);
        puts(ibt->buf);
        strbuf_free(ibt);
        free(bttext);
        return 0; /* OK */
    }

    /* Try to parse the backtrace. */
    struct backtrace *backtrace;
    backtrace = backtrace_parse(bttext, arguments.debug_parser, arguments.debug_scanner);

    /* If the parser failed print independent backtrace. */
    if (!backtrace)
    {
        if (arguments.rate)
        {
            free(bttext);
            puts("0");
            /* Parsing failed, but the output can be used. */
            return EX_PARSINGFAILED;
        }
        struct strbuf *ibt = independent_backtrace(bttext);
        puts(ibt->buf);
        strbuf_free(ibt);
        free(bttext);
        /* Parsing failed, but the output can be used. */
        return EX_PARSINGFAILED;
    }

    free(bttext);

    /* [--rate] Get the quality of the full backtrace. */
    float q1 = backtrace_quality(backtrace);

    /* If a single thread is requested, remove all other threads. */
    int retval = 0;
    struct thread *crash_thread = NULL;
    if (arguments.single_thread)
    {
        crash_thread = backtrace_find_crash_thread(backtrace);
        if (crash_thread)
            backtrace_remove_threads_except_one(backtrace, crash_thread);
        else
        {
            fprintf(stderr, "Detection of crash thread failed.\n");
            /* THREAD DETECTION FAILED, BUT THE OUTPUT CAN BE USED */
            retval = EX_THREADDETECTIONFAILED;
        }
    }
    
    /* [--rate] Get the quality of the crash thread. */
    float q2 = backtrace_quality(backtrace);

    if (arguments.remove_noncrash_frames)
        backtrace_remove_noncrash_frames(backtrace);

    /* If a frame removal is requested, do it now. */
    if (arguments.frame_depth > 0)
        backtrace_limit_frame_depth(backtrace, arguments.frame_depth);

    /* Frame removal can be done before removing exit handlers */
    if (arguments.remove_exit_handlers > 0)
        backtrace_remove_exit_handlers(backtrace);

    /* [--rate] Get the quality of frames around the crash. */
    float q3 = backtrace_quality(backtrace);

    if (arguments.rate)
    {
        /* Compute and store backtrace rating. */
        /* Compute and store backtrace rating. The crash frame
           is more important that the others. The frames around 
           the crash are more important than the rest.  */
        float qtot = 0.25f * q1 + 0.35f * q2 + 0.4f * q3;

        /* Turn the quality to rating. */
        const char *rating;
        if (qtot < 0.6f)      rating = "0";
        else if (qtot < 0.7f) rating = "1";
        else if (qtot < 0.8f) rating = "2";
        else if (qtot < 0.9f) rating = "3";
        else                  rating = "4";
        puts(rating);
    }
    else
        backtrace_print_tree(backtrace, arguments.verbose);

    backtrace_free(backtrace);
    return retval;
}
