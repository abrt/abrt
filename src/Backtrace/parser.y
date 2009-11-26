%{ /* -*- mode: yacc -*-
/*
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "backtrace.h"
#include "strbuf.h"

struct backtrace *g_backtrace;

#define YYDEBUG 1
#define YYMAXDEPTH 10000000
void yyerror(char const *s)
{
  fprintf (stderr, "\nParser error: %s\n", s);
}

%}
     
/* This defines the type of yylval */
%union {
  struct backtrace *backtrace;
  struct thread *thread;
  struct frame *frame;
  char *str;
  int num;
  char c;

  struct strbuf *strbuf;
}

/* Bison declarations.  */
%token END 0 "end of file"

%type <backtrace> backtrace
%type <thread> threads 
               thread
%type <frame>  frames 
               frame 
               frame_head 
               frame_head_1 
               frame_head_2 
               frame_head_3 
               frame_head_4 
               frame_head_5
%type <strbuf> identifier 
               hexadecimal_digit_sequence 
               hexadecimal_number 
               file_name 
               file_location 
               function_call 
               function_name 
               digit_sequence 
               frame_address_in_function
               identifier_braces
               identifier_braces_inside
%type <c>      nondigit 
               digit 
               hexadecimal_digit 
               file_name_char 
               identifier_char 
               identifier_braces_inside_char
               ws
               ws_nonl
               '(' ')' '+' '-' '/' '.' '_' '~' '[' ']'
               'a' 'b' 'c' 'd' 'e' 'f' 'g' 'h' 'i' 'j' 'k' 'l' 
               'm' 'n' 'o' 'p' 'q' 'r' 's' 't' 'u' 'v' 'w' 'x' 'y' 'z' 
               'A' 'B' 'C' 'D' 'E' 'F' 'G' 'H' 'I' 'J' 'K' 'L' 'M' 'N' 
               'O' 'P' 'Q' 'R' 'S' 'T' 'U' 'V' 'W' 'X' 'Y' 'Z' 
               '0' '1' '2' '3' '4' '5' '6' '7' '8' '9'
               '\'' '`' ',' '#' '@' '<' '>' '=' ':' '"' ';' ' ' 
               '\n' '\t' '\\' '!' '*' '%' '|' '^' '&' '$'
%type <num>    frame_start

%destructor { thread_free($$); } <thread>
%destructor { frame_free($$); } <frame>
%destructor { strbuf_free($$); } <strbuf>

%start backtrace
%glr-parser
%error-verbose
%locations

%% /* The grammar follows.  */

backtrace : /* empty */ %dprec 1 
             { $$ = g_backtrace = backtrace_new(); }
          | threads wsa %dprec 2
             { 
               $$ = g_backtrace = backtrace_new(); 
               $$->threads = $1; 
             }
          |  frame_head wss threads wsa %dprec 4
             { 
               $$ = g_backtrace = backtrace_new(); 
               $$->threads = $3; 
               $$->crash = $1;
             }
          | frame wss threads wsa %dprec 3
             { 
               $$ = g_backtrace = backtrace_new(); 
               $$->threads = $3; 
               $$->crash = $1;
             }
;

threads   : thread             { $$ = $1; }
          | threads wsa thread { $$ = thread_add_sibling($1, $3); }
;

thread    : keyword_thread wss digit_sequence wsa '(' keyword_thread wss digit_sequence wsa ')' ':' wsa frames 
              { 
                $$ = thread_new(); 
                $$->frames = $13; 
		
		if (sscanf($3->buf, "%d", &$$->number) != 1)
		{
		  printf("Error while parsing thread number '%s'", $3->buf);
		  exit(5);
		}
		strbuf_free($3);
		strbuf_free($8);
              }
;

frames    : frame            { $$ = $1; }
          | frames wsa frame { $$ = frame_add_sibling($1, $3); }
;

frame : frame_head_1 wss variables %dprec 3
      | frame_head_2 wss variables %dprec 4
      | frame_head_3 wss variables %dprec 5 
      | frame_head_4 wss variables %dprec 2 
      | frame_head_5 wss variables %dprec 1
;

frame_head : frame_head_1 %dprec 3 
	   | frame_head_2 %dprec 4 
	   | frame_head_3 %dprec 5 
           | frame_head_4 %dprec 2 
           | frame_head_5 %dprec 1
;

frame_head_1 : frame_start wss function_call wsa keyword_at wss file_location
             { 
               $$ = frame_new(); 
               $$->number = $1; 
               $$->function = $3->buf; 
               strbuf_free_nobuf($3); 
	       $$->sourcefile = $7->buf;
               strbuf_free_nobuf($7); 
             }
;

frame_head_2 : frame_start wss frame_address_in_function wss keyword_at wss file_location
             { 
               $$ = frame_new(); 
               $$->number = $1; 
               $$->function = $3->buf; 
               strbuf_free_nobuf($3); 
	       $$->sourcefile = $7->buf;
               strbuf_free_nobuf($7); 
             }
;

frame_head_3 : frame_start wss frame_address_in_function wss keyword_from wss file_location
             { 
               $$ = frame_new(); 
               $$->number = $1; 
               $$->function = $3->buf; 
               strbuf_free_nobuf($3); 
	       $$->sourcefile = $7->buf;
               strbuf_free_nobuf($7); 
             }
;

frame_head_4 : frame_start wss frame_address_in_function
             { 
               $$ = frame_new(); 
               $$->number = $1; 
               $$->function = $3->buf; 
               strbuf_free_nobuf($3); 
             }
;

frame_head_5 : frame_start wss keyword_sighandler
             { 
               $$ = frame_new(); 
               $$->number = $1; 
	       $$->signal_handler_called = true;
             }

frame_start: '#' digit_sequence 
                 { 
                   if (sscanf($2->buf, "%d", &$$) != 1)
		   {
		     printf("Error while parsing frame number '%s'.\n", $2->buf);
		     exit(5);
		   }
		   strbuf_free($2); 
                 }
;

frame_address_in_function : hexadecimal_number wss keyword_in wss function_call 
                              {
                                strbuf_free($1); 
                                $$ = $5; 
                              }
;

file_location : file_name ':' digit_sequence 
                  {
                    $$ = $1; 
                    strbuf_free($3); /* line number not needed for now */
                  }
              | file_name
;

variables : variables_line '\n'
          | variables_line END
          | variables_line wss_nonl '\n'
          | variables_line wss_nonl END
          | variables variables_line '\n'
          | variables variables_line END
          | variables wss_nonl variables_line '\n'
          | variables wss_nonl variables_line END
          | variables wss_nonl variables_line wss_nonl '\n'
          | variables wss_nonl variables_line wss_nonl END
;

variables_line : variables_char_no_framestart
               | variables_line variables_char
               | variables_line wss_nonl variables_char
;

variables_char : '#' | variables_char_no_framestart
;

/* Manually synchronized with function_args_char, except the first line. */
variables_char_no_framestart : digit | nondigit | '"' | '(' | ')'
                             | '+' | '-' | '<' | '>' | '/' | '.' 
                             | '[' | ']' | '?' | '\'' | '`' | ',' 
                             | '=' | '{' | '}' | '^' | '&' | '$'
                             | ':' | ';' | '\\' | '!' | '@' | '*' 
                             | '%' | '|' | '~'
;

function_call : function_name wss function_args
;

function_name : identifier
              | '?' '?' 
                { 
                  $$ = strbuf_new(); 
                  strbuf_append_str($$, "??"); 
                }
;

function_args : '(' wsa ')'
              | '(' wsa function_args_sequence wsa ')'
;

function_args_sequence : function_args_char
                       | function_args_sequence wsa '(' wsa ')'
                       | function_args_sequence wsa '(' wsa function_args_sequence wsa ')'
                       | function_args_sequence wsa function_args_char
                       | function_args_sequence wsa function_args_string
;

function_args_string : '"' wsa function_args_string_sequence wsa '"'
                     | '"' wsa '"'
;

/* Manually synchronized with variables_char_no_framestart,
 * except the first line. 
 */
function_args_char : digit | nondigit | '#'
                   | '+' | '-' | '<' | '>' | '/' | '.' 
                   | '[' | ']' | '?' | '\'' | '`' | ',' 
                   | '=' | '{' | '}' | '^' | '&' | '$'
                   | ':' | ';' | '\\' | '!' | '@' | '*' 
                   | '%' | '|' | '~'
;

function_args_string_sequence : function_args_string_char
                    | function_args_string_sequence function_args_string_char
                    | function_args_string_sequence wss_nonl function_args_string_char
;

function_args_string_char : function_args_char | '(' | ')'
;


file_name : file_name_char { $$ = strbuf_new(); strbuf_append_char($$, $1); }
          | file_name file_name_char { $$ = strbuf_append_char($1, $2); }
;

file_name_char : digit | nondigit | '-' | '+' | '/' | '.'
;

 /* Function name, sometimes mangled.  
  * Example: something@GLIB_2_2
  * CClass::operator=
  */
identifier : nondigit  
              {
		$$ = strbuf_new(); 
		strbuf_append_char($$, $1); 
	      }
           | identifier_braces /* e.g. (anonymous namespace)::WorkerThread */
           | identifier identifier_char 
	      { $$ = strbuf_append_char($1, $2); }
           | identifier identifier_braces 
              { 
		$$ = strbuf_append_str($1, $2->buf); 
		strbuf_free($2);
	      }
;

/* Most of the special characters are required to support C++ 
 * operator overloading.
 */
identifier_char : digit | nondigit | '@' | '.' | ':' | '=' 
		| '!' | '>' | '<' | '*' | '+' | '-' | '[' | ']'
                | '~' | '&' | '/' | '%' | '^'
                | '|' | ','
;

identifier_braces : '(' ')'
                      {
			$$ = strbuf_new();
			strbuf_append_char($$, $1);
			strbuf_append_char($$, $2);
		      }
                  | '(' identifier_braces_inside ')' 
                      {
			$$ = strbuf_new();
			strbuf_append_char($$, $1);
			strbuf_append_str($$, $2->buf);
			strbuf_free($2);
			strbuf_append_char($$, $3);
                      }
;

identifier_braces_inside : identifier_braces_inside_char 
                            {
	                      $$ = strbuf_new(); 
		              strbuf_append_char($$, $1); 
	                    }                         
                         | identifier_braces_inside identifier_braces_inside_char
			    { $$ = strbuf_append_char($1, $2); }
                         | identifier_braces_inside '(' identifier_braces_inside ')'
			    { 
			      $$ = strbuf_append_char($1, $2); 
			      $$ = strbuf_append_str($1, $3->buf); 
			      strbuf_free($3);
			      $$ = strbuf_append_char($1, $4); 
			    }
;

identifier_braces_inside_char : identifier_char | ws_nonl
;

digit_sequence : digit { $$ = strbuf_new(); strbuf_append_char($$, $1); }
               | digit_sequence digit { $$ = strbuf_append_char($1, $2); }
;

hexadecimal_number : '0' 'x' hexadecimal_digit_sequence 
                     { 
                       $$ = $3;
                       strbuf_prepend_str($$, "0x"); 
                     }
                   | '0' 'X' hexadecimal_digit_sequence 
                     { 
                       $$ = $3;
                       strbuf_prepend_str($$, "0X"); 
                     }
;

hexadecimal_digit_sequence : hexadecimal_digit 
                               { 
                                 $$ = strbuf_new(); 
                                 strbuf_append_char($$, $1); 
                               }
                           | hexadecimal_digit_sequence hexadecimal_digit 
                               { $$ = strbuf_append_char($1, $2); }
;

hexadecimal_digit : digit
                  | 'a' | 'b' | 'c' | 'd' | 'e' | 'f'
                  | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'
;

digit : '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
;

nondigit : 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i' | 'j' | 'k'
         | 'l' | 'm' | 'n' | 'o' | 'p' | 'q' | 'r' | 's' | 't' | 'u' | 'v' | 'w'
         | 'x' | 'y' | 'z' 
         | 'A' | 'B' | 'C' | 'D' | 'E' | 'F' | 'G' | 'H' | 'I' | 'J' | 'K'
         | 'L' | 'M' | 'N' | 'O' | 'P' | 'Q' | 'R' | 'S' | 'T' | 'U' | 'V' | 'W'
         | 'X' | 'Y' | 'Z'
         | '_'
;

 /* whitespace */
ws : ws_nonl | '\n' | '\r'
;

 /* No newline.*/
ws_nonl : '\t' | ' '
;

 /* whitespace sequence without a newline */
wss_nonl : ws_nonl
         | wss_nonl ws_nonl
;

 /* whitespace sequence */
wss : ws
    | wss ws
;
 
 /* whitespace sequence allowed */
wsa : 
    | wss
;

keyword_in : 'i' 'n'
;

keyword_at : 'a' 't'
;

keyword_from : 'f' 'r' 'o' 'm'
;

keyword_thread: 'T' 'h' 'r' 'e' 'a' 'd'
;

keyword_sighandler: '<' 's' 'i' 'g' 'n' 'a' 'l' ' ' 'h' 'a' 'n' 'd' 'l' 'e' 'r' ' ' 'c' 'a' 'l' 'l' 'e' 'd' '>'
;

%%

static bool scanner_echo = false;
static char *yyin;

int yylex()
{
  char c = *yyin;
  if (c == '\0')
    return END;
  ++yyin;

  /* Debug output. */
  if (scanner_echo)
    putchar(c);

  yylval.c = c;

  /* Return a single char. */
  return c;
}

/* This is the function that is actually called from outside. 
 * @returns
 *   Backtrace structure. Caller is responsible for calling
 *   backtrace_free() on this.
 *   Returns NULL when parsing failed.
 */
struct backtrace *do_parse(char *input, bool debug_parser, bool debug_scanner)
{
  /* Prepare for running parser. */
  g_backtrace = 0;
  yyin = input;
#if YYDEBUG == 1
  if (debug_parser)
    yydebug = 1;
#endif
  scanner_echo = debug_scanner;

  /* Parse. */
  int failure = yyparse();
  
  /* Separate debugging output. */
  if (scanner_echo)
    putchar('\n'); 

  if (failure)
  {
    if (g_backtrace)
    {
      backtrace_free(g_backtrace);
      g_backtrace = NULL;
    }
    fprintf(stderr, "Error while parsing backtrace.\n");
  }

  return g_backtrace;
}
