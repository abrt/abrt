/*
    utils.h

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
#ifndef BTPARSER_UTILS_H
#define BTPARSER_UTILS_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTP_lower "abcdefghijklmnopqrstuvwxyz"
#define BTP_upper "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define BTP_alpha BTP_lower BTP_upper
#define BTP_space " \t\r\n\v\f"
#define BTP_digit "0123456789"
#define BTP_alnum BTP_alpha BTP_digit

/**
 * Debugging output to stdout while parsing.
 * Default value is false.
 */
extern bool
btp_debug_parser;

/**
 * Never returns NULL.
 */
void *
btp_malloc(size_t size);

/**
 * Never returns NULL.
 */
char *
btp_vasprintf(const char *format, va_list p);

/**
 * Never returns NULL.
 */
char *
btp_strdup(const char *s);

/**
 * Never returns NULL.
 */
char *
btp_strndup(const char *s, size_t n);

/**
 * A strcmp() variant that works also with NULL parameters.  NULL is
 * considered to be less than a string.
 */
int
btp_strcmp0(const char *s1, const char *s2);

/**
 * A strchr() variant providing line and column in the string s
 * indicating where the char c was found.
 * @param line
 * Starts from 1.  Its value is valid only when this function does not
 * return NULL.
 * @param column
 * Starts from 0.  Its value is valid only when this function does not
 * return NULL.
 */
char *
btp_strchr_location(const char *s, int c, int *line, int *column);

/**
 * A strstr() variant providing line and column of the haystick
 * indicating where the needle was found.
 * @param line
 * Starts from 1.  Its value is valid only when this function does not
 * return NULL.
 * @param column
 * Starts from 0.  Its value is valid only when this function does not
 * return NULL.
 */
char *
btp_strstr_location(const char *haystack,
                    const char *needle,
                    int *line,
                    int *column);

/**
 * A strspn() variant providing line and column of the string s which
 * corresponds to the returned length.
 * @param line
 * Starts from 1.
 * @param column
 * Starts from 0.
 */
size_t
btp_strspn_location(const char *s,
                    const char *accept,
                    int *line,
                    int *column);

/**
 * Loads file contents to a string.
 * @returns
 * File contents. If file opening/reading fails, NULL is returned.
 */
char *
btp_file_to_string(const char *filename);

/**
 * If the input contains character c in the current positon, move the
 * input pointer after the character, and return true. Otherwise do
 * not modify the input and return false.
 */
bool
btp_skip_char(char **input, char c);

/**
 * If the input contains one of allowed characters, move
 * the input pointer after that character, and return true.
 * Otherwise do not modify the input and return false.
 */
bool
btp_skip_char_limited(char **input, const char *allowed);

/**
 * If the input contains one of allowed characters, store
 * the character to the result, move the input pointer after
 * that character, and return true. Otherwise do not modify
 * the input and return false.
 */
bool
btp_parse_char_limited(char **input, const char *allowed, char *result);

/**
 * If the input contains the character c one or more times, update it
 * so that the characters are skipped. Returns the number of characters
 * skipped, thus zero if **input does not contain c.
 */
int
btp_skip_char_sequence(char **input, char c);

/**
 * If the input contains one or more characters from string chars,
 * move the input pointer after the sequence. Otherwise do not modify
 * the input.
 * @returns
 * The number of characters skipped.
 */
int
btp_skip_char_span(char **input, const char *chars);

/**
 * If the input contains one or more characters from string chars,
 * move the input pointer after the sequence. Otherwise do not modify
 * the input.
 * @param line
 * Starts from 1. Corresponds to the returned number.
 * @param column
 * Starts from 0. Corresponds to the returned number.
 * @returns
 * The number of characters skipped.
 */
int
btp_skip_char_span_location(char **input,
                            const char *chars,
                            int *line,
                            int *column);

/**
 * If the input contains one or more characters from string accept,
 * create a string from this sequence and store it to the result, move
 * the input pointer after the sequence, and return the lenght of the
 * sequence.  Otherwise do not modify the input and return 0.
 *
 * If this function returns nonzero value, the caller is responsible
 * to free the result.
 */
int
btp_parse_char_span(char **input, const char *accept, char **result);

/**
 * If the input contains characters which are not in string reject,
 * create a string from this sequence and store it to the result,
 * move the input pointer after the sequence, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to
 * free the result.
 */
bool
btp_parse_char_cspan(char **input, const char *reject, char **result);

/**
 * If the input contains the string, move the input pointer after
 * the sequence. Otherwise do not modify the input.
 * @returns
 * Number of characters skipped. 0 if the input does not contain the
 * string.
 */
int
btp_skip_string(char **input, const char *string);

/**
 * If the input contains the string, copy the string to result,
 * move the input pointer after the string, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the result.
 */
bool
btp_parse_string(char **input, const char *string, char **result);

/**
 * If the input contains digit 0-9, return it as a character
 * and move the input pointer after it. Otherwise return
 * '\0' and do not modify the input.
 */
char
btp_parse_digit(char **input);

/**
 * If the input contains [0-9]+, move the input pointer
 * after the number.
 * @returns
 * The number of skipped characters. 0 if input does not start with a
 * digit.
 */
int
btp_skip_unsigned_integer(char **input);

/**
 * If the input contains [0-9]+, parse it, move the input pointer
 * after the number.
 * @returns
 * Number of parsed characters. 0 if input does not contain a number.
 */
int
btp_parse_unsigned_integer(char **input, unsigned *result);

/**
 * If the input contains 0x[0-9a-f]+, move the input pointer
 * after that.
 * @returns
 * The number of characters processed from input. 0 if the input does
 * not contain a hexadecimal number.
 */
int
btp_skip_hexadecimal_number(char **input);

/**
 * If the input contains 0x[0-9a-f]+, parse the number, and move the
 * input pointer after it.  Otherwise do not modify the input.
 * @returns
 * The number of characters read from input. 0 if the input does not
 * contain a hexadecimal number.
 */
int
btp_parse_hexadecimal_number(char **input, uint64_t *result);

#ifdef __cplusplus
}
#endif

#endif
