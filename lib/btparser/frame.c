/*
    frame.c

    Copyright (C) 2010  Red Hat, Inc.

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
#include "frame.h"
#include "strbuf.h"
#include "utils.h"
#include "location.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct btp_frame *
btp_frame_new()
{
    struct btp_frame *frame = btp_malloc(sizeof(struct btp_frame));
    btp_frame_init(frame);
    return frame;
}

void
btp_frame_init(struct btp_frame *frame)
{
    frame->function_name = NULL;
    frame->function_type = NULL;
    frame->number = 0;
    frame->source_file = NULL;
    frame->source_line = -1;
    frame->signal_handler_called = false;
    frame->address = -1;
    frame->next = NULL;
}

void
btp_frame_free(struct btp_frame *frame)
{
    if (!frame)
        return;
    free(frame->function_name);
    free(frame->function_type);
    free(frame->source_file);
    free(frame);
}

struct btp_frame *
btp_frame_dup(struct btp_frame *frame, bool siblings)
{
    struct btp_frame *result = btp_frame_new();
    memcpy(result, frame, sizeof(struct btp_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings if the copy is not shallow. */
    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);
    if (result->function_type)
        result->function_type = btp_strdup(result->function_type);
    if (result->source_file)
        result->source_file = btp_strdup(result->source_file);

    return result;
}

bool
btp_frame_calls_func(struct btp_frame *frame,
                     const char *function_name)
{
    return frame->function_name &&
        0 == strcmp(frame->function_name, function_name);
}

bool
btp_frame_calls_func_in_file(struct btp_frame *frame,
                             const char *function_name,
                             const char *source_file)
{
    return frame->function_name &&
        0 == strcmp(frame->function_name, function_name) &&
        frame->source_file &&
        NULL != strstr(frame->source_file, source_file);
}

bool
btp_frame_calls_func_in_file2(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1)
{
    return frame->function_name &&
        0 == strcmp(frame->function_name, function_name) &&
        frame->source_file &&
        (NULL != strstr(frame->source_file, source_file0) ||
         NULL != strstr(frame->source_file, source_file1));
}

bool
btp_frame_calls_func_in_file3(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1,
                              const char *source_file2)
{
    return frame->function_name &&
        0 == strcmp(frame->function_name, function_name) &&
        frame->source_file &&
        (NULL != strstr(frame->source_file, source_file0) ||
         NULL != strstr(frame->source_file, source_file1) ||
         NULL != strstr(frame->source_file, source_file2));
}

bool
btp_frame_calls_func_in_file4(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1,
                              const char *source_file2,
                              const char *source_file3)
{
    return frame->function_name &&
        0 == strcmp(frame->function_name, function_name) &&
        frame->source_file &&
        (NULL != strstr(frame->source_file, source_file0) ||
         NULL != strstr(frame->source_file, source_file1) ||
         NULL != strstr(frame->source_file, source_file2) ||
         NULL != strstr(frame->source_file, source_file3));
}

int
btp_frame_cmp(struct btp_frame *f1,
              struct btp_frame *f2,
              bool compare_number)
{
    /* Singnal handler. */
    if (f1->signal_handler_called)
    {
        if (!f2->signal_handler_called)
            return 1;

        /* Both contain signal handler called. */
        return 0;
    }
    else
    {
        if (f2->signal_handler_called)
            return -1;
        /* No signal handler called, continue. */
    }

    /* Function. */
    int function_name = btp_strcmp0(f1->function_name, f2->function_name);
    if (function_name != 0)
        return function_name;
    int function_type = btp_strcmp0(f1->function_type, f2->function_type);
    if (function_type != 0)
        return function_type;

    /* Sourcefile. */
    int source_file = btp_strcmp0(f1->source_file, f2->source_file);
    if (source_file != 0)
        return source_file;

    /* Sourceline. */
    int source_line = f1->source_line - f2->source_line;
    if (source_line != 0)
        return source_line;

    /* Frame number. */
    if (compare_number)
    {
        int number = f1->number - f2->number;
        if (number != 0)
            return number;
    }

    return 0;
}

void
btp_frame_add_sibling(struct btp_frame *a, struct btp_frame *b)
{
    struct btp_frame *aa = a;
    while (aa->next)
        aa = aa->next;

    aa->next = b;
}

void
btp_frame_append_to_str(struct btp_frame *frame,
                        struct strbuf *str,
                        bool verbose)
{
    if (verbose)
        strbuf_append_strf(str, " #%d", frame->number);
    else
        strbuf_append_str(str, " ");

    if (frame->function_type)
        strbuf_append_strf(str, " %s", frame->function_type);
    if (frame->function_name)
        strbuf_append_strf(str, " %s", frame->function_name);
    if (verbose && frame->source_file)
    {
        if (frame->function_name)
            strbuf_append_str(str, " at");
        strbuf_append_strf(str, " %s", frame->source_file);
    }

    if (frame->signal_handler_called)
        strbuf_append_str(str, " <signal handler called>");

    strbuf_append_str(str, "\n");
}

/**
 * Find string a or b in input, whatever comes first.
 * If no string is found, return the \0 character at the end of input.
 */
static char *
findfirstabnul(char *input, const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    char *p = input;
    while (*p)
    {
        if (strncmp(p, a, alen) == 0)
            return p;
        if (strncmp(p, b, blen) == 0)
            return p;
        ++p;
    }
    return p;
}

struct btp_frame *
btp_frame_parse(char **input,
                struct btp_location *location)
{
    char *local_input = *input;
    struct btp_frame *header = btp_frame_parse_header(input, location);
    if (!header)
        return NULL;

    /* Skip the variables section for now. */
    /* Todo: speedup by implementing strstrnul. */
    local_input = findfirstabnul(local_input, "\n#", "\nThread");
    if (*local_input != '\0')
        ++local_input; /* ++ skips the newline */

    if (btp_debug_parser)
    {
        printf("frame #%u %s\n",
               header->number,
               header->function_name ? header->function_name : "signal handler called");
    }

    *input = local_input;
    return header;
}

int
btp_frame_parse_frame_start(char **input, unsigned *number)
{
    char *local_input = *input;

    /* Read the hash sign. */
    if (!btp_skip_char(&local_input, '#'))
        return 0;
    int count = 1;

    /* Read the frame position. */
    int digits = btp_parse_unsigned_integer(&local_input, number);
    count += digits;
    if (0 == digits)
        return 0;

    /* Read all the spaces after the positon. */
    int spaces = btp_skip_char_sequence(&local_input, ' ');
    count += spaces;
    if (0 == spaces)
        return 0;

    *input = local_input;
    return count;
}

int
btp_frame_parseadd_operator(char **input, struct strbuf *target)
{
    char *local_input = *input;
    if (0 == btp_skip_string(&local_input, "operator"))
        return 0;

#define OP(x) \
    if (0 < btp_skip_string(&local_input, x))      \
    {                                              \
        strbuf_append_str(target, "operator"); \
        strbuf_append_str(target, x);          \
        int length = local_input - *input;         \
        *input = local_input;                      \
        return length;                             \
    }

    OP(">>=")OP(">>")OP(">=")OP(">");
    OP("<<=")OP("<<")OP("<=")OP("<");
    OP("->*")OP("->")OP("-");
    OP("==")OP("=");
    OP("&&")OP("&=")OP("&");
    OP("||")OP("|=")OP("|");
    OP("++")OP("+=")OP("+");
    OP("--")OP("-=")OP("-");
    OP("/=")OP("/");
    OP("*=")OP("*");
    OP("%=")OP("%");
    OP("!=")OP("!");
    OP("~");
    OP("()");
    OP("[]");
    OP(",");
    OP("^=")OP("^");
    OP(" new[]")OP(" new");
    OP(" delete[]")OP(" delete");
    /* User defined operators are not parsed.
       Should they be? */
#undef OP
    return 0;
}

#define FUNCTION_NAME_CHARS BTP_alnum "@.:=!*+-[]~&/%^|,_"

int
btp_frame_parse_function_name_chunk(char **input,
                                    bool space_allowed,
                                    char **target)
{
    char *local_input = *input;
    struct strbuf *buf = strbuf_new();
    while (*local_input)
    {
        if (0 < btp_frame_parseadd_operator(&local_input, buf))
        {
            /* Space is allowed after operator even when it
               is not normally allowed. */
            if (btp_skip_char(&local_input, ' '))
            {
                /* ...but if ( follows, it is not allowed. */
                if (btp_skip_char(&local_input, '('))
                {
                    /* Return back both the space and (. */
                    local_input -= 2;
                }
                else
                    strbuf_append_char(buf, ' ');
            }
        }

        if (strchr(FUNCTION_NAME_CHARS, *local_input) == NULL)
        {
            if (!space_allowed || strchr(" ", *local_input) == NULL)
                break;
        }

        strbuf_append_char(buf, *local_input);
        ++local_input;
    }

    if (buf->len == 0)
    {
        strbuf_free(buf);
        return 0;
    }

    *target = strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

int
btp_frame_parse_function_name_braces(char **input, char **target)
{
    char *local_input = *input;
    if (!btp_skip_char(&local_input, '('))
        return 0;

    struct strbuf *buf = strbuf_new();
    strbuf_append_char(buf, '(');
    while (true)
    {
        char *namechunk = NULL;
        if (0 < btp_frame_parse_function_name_chunk(&local_input, true, &namechunk) ||
            0 < btp_frame_parse_function_name_braces(&local_input, &namechunk) ||
            0 < btp_frame_parse_function_name_template(&local_input, &namechunk))
        {
            strbuf_append_str(buf, namechunk);
            free(namechunk);
        }
        else
            break;
    }

    if (!btp_skip_char(&local_input, ')'))
    {
        strbuf_free(buf);
        return 0;
    }

    strbuf_append_char(buf, ')');
    *target = strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

int
btp_frame_parse_function_name_template(char **input, char **target)
{
    char *local_input = *input;
    if (!btp_skip_char(&local_input, '<'))
        return 0;

    struct strbuf *buf = strbuf_new();
    strbuf_append_char(buf, '<');
    while (true)
    {
        char *namechunk = NULL;
        if (0 < btp_frame_parse_function_name_chunk(&local_input, true, &namechunk) ||
            0 < btp_frame_parse_function_name_braces(&local_input, &namechunk) ||
            0 < btp_frame_parse_function_name_template(&local_input, &namechunk))
        {
            strbuf_append_str(buf, namechunk);
            free(namechunk);
        }
        else
            break;
    }

    if (!btp_skip_char(&local_input, '>'))
    {
        strbuf_free(buf);
        return 0;
    }

    strbuf_append_char(buf, '>');
    *target = strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

bool
btp_frame_parse_function_name(char **input,
                              char **function_name,
                              char **function_type,
                              struct btp_location *location)
{
    /* Handle unknown function name, represended by double question
       mark. */
    if (btp_parse_string(input, "??", function_name))
    {
        *function_type = NULL;
        location->column += 2;
        return true;
    }

    char *local_input = *input;
    /* Up to three parts of function name. */
    struct strbuf *buf0 = strbuf_new(), *buf1 = NULL;

    /* First character:
       '~' for destructor
       '*' for ????
       '_a-zA-Z' for mangled/nonmangled function name
       '(' to start "(anonymous namespace)::" or something
     */
    char first;
    char *namechunk;
    if (btp_parse_char_limited(&local_input, "~*_" BTP_alpha, &first))
    {
        /* If it's a start of 'o'perator, put the 'o' back! */
        if (first == 'o')
            --local_input;
        else
        {
            strbuf_append_char(buf0, first);
            ++location->column;
        }
    }
    else
    {
        int chars = btp_frame_parse_function_name_braces(&local_input,
                                                         &namechunk);
        if (0 < chars)
        {
            strbuf_append_str(buf0, namechunk);
            free(namechunk);
            location->column += chars;
        }
        else
        {
            location->message = "Expected function name.";
            strbuf_free(buf0);
            return false;
        }
    }

    /* The rest consists of function name, braces, templates...*/
    while (true)
    {
        char *namechunk = NULL;
        int chars = btp_frame_parse_function_name_chunk(&local_input,
                                                        false,
                                                        &namechunk);

        if (0 == chars)
        {
            chars = btp_frame_parse_function_name_braces(&local_input,
                                                         &namechunk);
        }

        if (0 == chars)
        {
            chars = btp_frame_parse_function_name_template(&local_input,
                                                           &namechunk);
        }

        if (0 == chars)
            break;

        strbuf_append_str(buf0, namechunk);
        free(namechunk);
        location->column += chars;
    }

    /* Function name MUST be ended by empty space. */
    char space;
    if (!btp_parse_char_limited(&local_input, BTP_space, &space))
    {
        strbuf_free(buf0);
        location->message = "Space or newline expected after function name.";
        return false;
    }

    /* Some C++ function names and function types might contain suffix
       " const". */
    int chars = btp_skip_string(&local_input, "const");
    if (0 < chars)
    {
        strbuf_append_char(buf0, space);
        btp_location_eat_char(location, space);
        strbuf_append_str(buf0, "const");
        location->column += chars;

        /* Check the empty space after function name again.*/
        if (!btp_parse_char_limited(&local_input, BTP_space, &space))
        {
            /* Function name MUST be ended by empty space. */
            strbuf_free(buf0);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Maybe the first series was just a type of the function, and now
       the real function follows. Now, we know it must not start with
       '(', nor with '<'. */
    chars = btp_frame_parse_function_name_chunk(&local_input,
                                                false,
                                                &namechunk);
    if (0 < chars)
    {
        /* Eat the space separator first. */
        btp_location_eat_char(location, space);

        buf1 = strbuf_new();
        strbuf_append_str(buf1, namechunk);
        free(namechunk);
        location->column += chars;

        /* The rest consists of a function name parts, braces, templates...*/
        while (true)
        {
            char *namechunk = NULL;
            chars = btp_frame_parse_function_name_chunk(&local_input,
                                                        false,
                                                        &namechunk);
            if (0 == chars)
            {
                chars = btp_frame_parse_function_name_braces(&local_input,
                                                             &namechunk);
            }
            if (0 == chars)
            {
                chars = btp_frame_parse_function_name_template(&local_input,
                                                               &namechunk);
            }
            if (0 == chars)
                break;

            strbuf_append_str(buf1, namechunk);
            free(namechunk);
            location->column += chars;
        }

        /* Function name MUST be ended by empty space. */
        if (!btp_parse_char_limited(&local_input, BTP_space, &space))
        {
            strbuf_free(buf0);
            strbuf_free(buf1);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Again, some C++ function names might contain suffix " const" */
    chars = btp_skip_string(&local_input, "const");
    if (0 < chars)
    {
        struct strbuf *buf = buf1 ? buf1 : buf0;
        strbuf_append_char(buf, space);
        btp_location_eat_char(location, space);
        strbuf_append_str(buf, "const");
        location->column += chars;

        /* Check the empty space after function name again.*/
        if (!btp_skip_char_limited(&local_input, BTP_space))
        {
            /* Function name MUST be ended by empty space. */
            strbuf_free(buf0);
            strbuf_free(buf1);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Return back to the empty space. */
    --local_input;

    if (buf1)
    {
        *function_name = strbuf_free_nobuf(buf1);
        *function_type = strbuf_free_nobuf(buf0);
    }
    else
    {
        *function_name = strbuf_free_nobuf(buf0);
        *function_type = NULL;
    }

    *input = local_input;
    return true;
}

bool
btp_frame_skip_function_args(char **input, struct btp_location *location)
{
    char *local_input = *input;
    if (!btp_skip_char(&local_input, '('))
    {
        location->message = "Expected '(' to start function argument list.";
        return false;
    }
    location->column += 1;

    int depth = 0;
    bool string = false;
    bool escape = false;
    do
    {
        if (string)
        {
            if (escape)
                escape = false;
            else if (*local_input == '\\')
                escape = true;
            else if (*local_input == '"')
                string = false;
        }
        else
        {
            if (*local_input == '"')
                string = true;
            else if (*local_input == '(')
                ++depth;
            else if (*local_input == ')')
            {
                if (depth > 0)
                    --depth;
                else
                    break;
            }
        }
        btp_location_eat_char(location, *local_input);
        ++local_input;
    }
    while (*local_input);

    if (depth != 0 || string || escape)
    {
        location->message = "Unbalanced function parameter list.";
        return false;
    }

    if (!btp_skip_char(&local_input, ')'))
    {
        location->message = "Expected ')' to close the function parameter list.";
        return false;
    }
    location->column += 1;

    *input = local_input;
    return true;
}

bool
btp_frame_parse_function_call(char **input,
                              char **function_name,
                              char **function_type,
                              struct btp_location *location)
{
    char *local_input = *input;
    char *name = NULL, *type = NULL;
    if (!btp_frame_parse_function_name(&local_input,
                                       &name,
                                       &type,
                                       location))
    {
        /* The location message is set by the function returning
         * false, no need to update it here. */
        return false;
    }

    int line, column;
    if (0 == btp_skip_char_span_location(&local_input,
                                         " \n",
                                         &line,
                                         &column))
    {
        free(name);
        free(type);
        location->message = "Expected a space or newline after the function name.";
        return false;
    }
    btp_location_add(location, line, column);

    if (!btp_frame_skip_function_args(&local_input, location))
    {
        free(name);
        free(type);
        /* The location message is set by the function returning
         * false, no need to update it here. */
        return false;
    }

    *function_name = name;
    *function_type = type;
    *input = local_input;
    return true;
}

bool
btp_frame_parse_address_in_function(char **input,
                                    uint64_t *address,
                                    char **function_name,
                                    char **function_type,
                                    struct btp_location *location)
{
    char *local_input = *input;

    /* Read memory address in hexadecimal format. */
    int digits = btp_parse_hexadecimal_number(&local_input, address);
    location->column += digits;
    if (0 == digits)
    {
        location->message = "Hexadecimal number representing memory address expected.";
        return false;
    }

    /* Skip spaces. */
    int chars = btp_skip_char_sequence(&local_input, ' ');
    location->column += chars;
    if (0 == chars)
    {
        location->message = "Space expected after memory address.";
        return false;
    }

    /* Skip keyword "in". */
    chars = btp_skip_string(&local_input, "in");
    location->column += chars;
    if (0 == chars)
    {
        location->message = "Keyword \"in\" expected after memory address.";
        return false;
    }

    /* Skip spaces. */
    chars = btp_skip_char_sequence(&local_input, ' ');
    location->column += chars;
    if (0 == chars)
    {
        location->message = "Space expected after 'in'.";
        return false;
    }

    /* C++ specific case for "0xfafa in vtable for function ()" */
    chars = btp_skip_string(&local_input, "vtable");
    location->column += chars;
    if (0 <  chars)
    {
        chars = btp_skip_char_sequence(&local_input, ' ');
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Space expected after 'vtable'.";
            return false;
        }

        chars = btp_skip_string(&local_input, "for");
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Keyword \"for\" expected.";
            return false;
        }

        chars = btp_skip_char_sequence(&local_input, ' ');
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Space expected after 'for'.";
            return false;
        }
    }

    if (!btp_frame_parse_function_call(&local_input,
                                       function_name,
                                       function_type,
                                       location))
    {
        /* Do not update location here, it has been modified by the
           called function. */
        return false;
    }

    *input = local_input;
    return true;
}

bool
btp_frame_parse_file_location(char **input,
                              char **file,
                              unsigned *fileline,
                              struct btp_location *location)
{
    char *local_input = *input;
    int line, column;
    if (0 == btp_skip_char_span_location(&local_input, " \n", &line, &column))
    {
        location->message = "Expected a space or a newline.";
        return false;
    }
    btp_location_add(location, line, column);

    int chars = btp_skip_string(&local_input, "at");
    if (0 == chars)
    {
        chars = btp_skip_string(&local_input, "from");
        if (0 == chars)
        {
            location->message = "Expected 'at' or 'from'.";
            return false;
        }
    }
    location->column += chars;

    int spaces = btp_skip_char_sequence(&local_input, ' ');
    location->column += spaces;
    if (0 == spaces)
    {
        location->message = "Expected a space before file location.";
        return false;
    }

    char *file_name;
    chars = btp_parse_char_span(&local_input, BTP_alnum "_/\\+.-", &file_name);
    location->column += chars;
    if (0 == chars)
    {
        location->message = "Expected a file name.";
        return false;
    }

    if (btp_skip_char(&local_input, ':'))
    {
        location->column += 1;
        int digits = btp_parse_unsigned_integer(&local_input, fileline);
        location->column += digits;
        if (0 == digits)
        {
            free(file_name);
            location->message = "Expected a line number.";
            return false;
        }
    }
    else
        *fileline = -1;

    *file = file_name;
    *input = local_input;
    return true;
}

struct btp_frame *
btp_frame_parse_header(char **input,
                       struct btp_location *location)
{
    char *local_input = *input;
    struct btp_frame *imframe = btp_frame_new(); /* im - intermediate */
    int chars = btp_frame_parse_frame_start(&local_input,
                                            &imframe->number);

    location->column += chars;
    if (0 == chars)
    {
        location->message = "Frame start sequence expected.";
        btp_frame_free(imframe);
        return NULL;
    }

    struct btp_location internal_location;
    btp_location_init(&internal_location);
    if (btp_frame_parse_address_in_function(&local_input,
                                            &imframe->address,
                                            &imframe->function_name,
                                            &imframe->function_type,
                                            &internal_location))
    {
        btp_location_add(location,
                         internal_location.line,
                         internal_location.column);

        /* Optional section " from file.c:65" */
        /* Optional section " at file.c:65" */
        btp_location_init(&internal_location);
        if (btp_frame_parse_file_location(&local_input,
                                          &imframe->source_file,
                                          &imframe->source_line,
                                          &internal_location))
        {
            btp_location_add(location,
                             internal_location.line,
                             internal_location.column);
        }
    }
    else
    {
        btp_location_init(&internal_location);
        if (btp_frame_parse_function_call(&local_input,
                                          &imframe->function_name,
                                          &imframe->function_type,
                                          &internal_location))
        {
            btp_location_add(location,
                             internal_location.line,
                             internal_location.column);

            /* Mandatory section " at file.c:65" */
            btp_location_init(&internal_location);
            if (!btp_frame_parse_file_location(&local_input,
                                               &imframe->source_file,
                                               &imframe->source_line,
                                               &internal_location))
            {
                location->message = "Function call in the frame header "
                    "misses mandatory \"at file.c:xy\" section";
                btp_frame_free(imframe);
                return NULL;
            }

            btp_location_add(location,
                             internal_location.line,
                             internal_location.column);
        }
        else
        {
            int chars = btp_skip_string(&local_input, "<signal handler called>");
            if (0 < chars)
            {
                location->column += chars;
                imframe->signal_handler_called = true;
            }
            else
            {
                location->message = "Frame header variant not recognized.";
                btp_frame_free(imframe);
                return NULL;
            }
        }
    }

    *input = local_input;
    return imframe;
}
