/*
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
#include "config.h"
#include "backtrace.h"
#include "fallback.h"

#define EX_PARSINGFAILED EX__MAX + 1
#define EX_THREADDETECTIONFAILED EX__MAX + 2

const char *argp_program_version = "abrt-backtrace " VERSION;
const char *argp_program_bug_address = "<crash-catcher@lists.fedorahosted.org>";

static char doc[] = "abrt-backtrace -- backtrace analyzer";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE";

static struct argp_option options[] = {
  {"independent"          , 'i', 0  , 0, "Prints independent backtrace (fallback)" },
  {"single-thread"        , 'n', 0  , 0, "Display the crash thread only in the backtrace" },
  {"frame-depth"          , 'd', "N", 0, "Display only top N frames under the crash frame" },
  {"remove-exit-handlers" , 'r', 0  , 0, "Removes exit handlers from the displayed backtrace" },
  {"debug-parser"         , 'p', 0  , 0, "Prints parser debug information"}, 
  {"debug-scanner"        , 's', 0  , 0, "Prints scanner debug information"}, 
  {"verbose"              , 'v', 0  , 0, "Print human-friendly superfluous output."}, 
  { 0 }
};

struct arguments
{
  bool independent;
  bool single_thread;
  int frame_depth; /* negative == do not limit the depth */
  bool remove_exit_handlers;
  bool debug_parser;
  bool debug_scanner;
  bool verbose;
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
  case 'p': arguments->debug_parser = true; break;
  case 's': arguments->debug_scanner = true; break;
  case 'v': arguments->verbose = true; break;

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
    if (!arguments->filename)
    {
      /* Not enough arguments. */
      argp_usage(state);
      exit(EX_USAGE); /* is there a better errno? */
    }
    break;
    
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };


int main(int argc, char **argv)
{
  /* Set options default values and parse program command line. */
  struct arguments arguments;
  arguments.independent = false;
  arguments.frame_depth = -1;
  arguments.single_thread = false;
  arguments.remove_exit_handlers = false;
  arguments.debug_parser = false;
  arguments.debug_scanner = false;
  arguments.verbose = false;
  arguments.filename = 0;
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  /* Open input file and parse it. */
  FILE *fp = fopen(arguments.filename, "r");
  if (!fp)
  {
    fprintf(stderr, "Unable to open '%s'.\n", arguments.filename);
    exit(EX_NOINPUT); /* No such file or directory */
  }

  /* Print independent backtrace and exit. */
  if (arguments.independent)
  {
    struct strbuf *ibt = independent_backtrace(fp);
    fclose(fp);
    puts(ibt->buf);
    strbuf_free(ibt);
    return 0; /* OK */
  }

  /* Try to parse the backtrace. */
  struct backtrace *backtrace;
  backtrace = do_parse(fp, arguments.debug_parser, arguments.debug_scanner);

  /* If the parser failed print independent backtrace. */
  if (!backtrace)
  {
    fseek(fp, 0, SEEK_SET);
    struct strbuf *ibt = independent_backtrace(fp);
    fclose(fp);
    puts(ibt->buf);
    strbuf_free(ibt);
    /* PARSING FAILED, BUT OUTPUT CAN BE USED */
    return EX_PARSINGFAILED; 
  }

  fclose(fp);

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

  /* If a frame removal is requested, do it now. */
  if (arguments.frame_depth > 0)
    backtrace_limit_frame_depth(backtrace, arguments.frame_depth);    

  /* Frame removal can be done before removing exit handlers */
  if (arguments.remove_exit_handlers > 0)
    backtrace_remove_exit_handlers(backtrace);

  backtrace_print_tree(backtrace, arguments.verbose);
  backtrace_free(backtrace);
  return retval;
}
