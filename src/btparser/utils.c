/*
    utils.c

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
#include "utils.h"
#include "location.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

bool btp_debug_parser = false;

void *
btp_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        fprintf(stderr, "btp: out of memory");
        exit(1);
    }
    return ptr;
}

char *
btp_vasprintf(const char *format, va_list p)
{
    int r;
    char *string_ptr;

#if 1
    // GNU extension
    r = vasprintf(&string_ptr, format, p);
#else
    // Bloat for systems that haven't got the GNU extension.
    va_list p2;
    va_copy(p2, p);
    r = vsnprintf(NULL, 0, format, p);
    string_ptr = xmalloc(r+1);
    r = vsnprintf(string_ptr, r+1, format, p2);
    va_end(p2);
#endif

    if (r < 0)
    {
        fprintf(stderr, "btp: out of memory");
        exit(1);
    }

    return string_ptr;
}

char *
btp_strdup(const char *s)
{
    return btp_strndup(s, strlen(s));
}

char *
btp_strndup(const char *s, size_t n)
{
    char *result = strndup(s, n);
    if (result == NULL)
    {
        fprintf(stderr, "btp: out of memory");
        exit(1);
    }
    return result;
}

int
btp_strcmp0(const char *s1, const char *s2)
{
    if (!s1)
    {
        if (s2)
            return -1;
        return 0;
    }
    else
    {
        if (!s2)
            return 1;
        /* Both are non-null. */
        return strcmp(s1, s2);
    }
}

char *
btp_strchr_location(const char *s, int c, int *line, int *column)
{
    *line = 1;
    *column = 0;

    /* Scan s for the character.  When this loop is finished,
       s will either point to the end of the string or the
       character we were looking for.  */
    while (*s != '\0' && *s != (char)c)
    {
        btp_location_eat_char_ext(line, column, *s);
        ++s;
    }
    return ((*s == c) ? (char*)s : NULL);
}

char *
btp_strstr_location(const char *haystack,
                    const char *needle,
                    int *line,
                    int *column)
{
    *line = 1;
    *column = 0;
    size_t needlelen;

    /* Check for the null needle case.  */
    if (*needle == '\0')
        return (char*)haystack;

    needlelen = strlen(needle);
    int chrline, chrcolumn;
    for (;(haystack = btp_strchr_location(haystack, *needle, &chrline, &chrcolumn)) != NULL; ++haystack)
    {
        btp_location_add_ext(line, column, chrline, chrcolumn);

        if (strncmp(haystack, needle, needlelen) == 0)
            return (char*)haystack;

        btp_location_eat_char_ext(line, column, *haystack);
    }
    return NULL;
}

size_t
btp_strspn_location(const char *s,
                    const char *accept,
                    int *line,
                    int *column)
{
    *line = 1;
    *column = 0;
    const char *sc;
    for (sc = s; *sc != '\0'; ++sc)
    {
        if (strchr(accept, *sc) == NULL)
            return (sc - s);

        btp_location_eat_char_ext(line, column, *sc);
    }
    return sc - s; /* terminating nulls don't match */
}

char *
btp_file_to_string(const char *filename)
{
    /* Open input file, and parse it. */
    int fd = open(filename, O_RDONLY | O_LARGEFILE);
    if (fd < 0)
    {
        fprintf(stderr, "Unable to open '%s': %s.\n",
                filename, strerror(errno));
        return NULL;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) /* EOVERFLOW? */
    {
        fprintf(stderr, "Unable to seek in '%s': %s.\n",
                filename, strerror(errno));
    }

    lseek(fd, 0, SEEK_SET); /* No reason to fail. */

    static const size_t FILE_SIZE_LIMIT = 20000000; /* ~ 20 MB */
    if (size > FILE_SIZE_LIMIT)
    {
        fprintf(stderr, "Input file too big (%lld). Maximum size is %zd.\n",
                (long long)size, FILE_SIZE_LIMIT);
        close(fd);
        return NULL;
    }

    char *contents = btp_malloc(size + 1);
    if (size != read(fd, contents, size))
    {
        fprintf(stderr, "Unable to read from '%s'.\n", filename);
        close(fd);
        free(contents);
        return NULL;
    }

    /* Just reading, so no need to check the returned value. */
    close(fd);

    contents[size] = '\0';
    return contents;
}

bool
btp_skip_char(char **input, char c)
{
    if (**input != c)
        return false;
    ++*input;
    return true;
}

bool
btp_skip_char_limited(char **input, const char *allowed)
{
    if (strchr(allowed, **input) == NULL)
        return false;
    ++*input;
    return true;
}

bool
btp_parse_char_limited(char **input, const char *allowed, char *result)
{
    if (**input == '\0')
        return false;
    if (strchr(allowed, **input) == NULL)
        return false;
    *result = **input;
    ++*input;
    return true;
}

int
btp_skip_char_sequence(char **input, char c)
{
    int count = 0;

    /* Skip all the occurences of c. */
    while (btp_skip_char(input, c))
        ++count;

    return count;
}

int
btp_skip_char_span(char **input, const char *chars)
{
    size_t count = strspn(*input, chars);
    if (0 == count)
        return count;
    *input += count;
    return count;
}

int
btp_skip_char_span_location(char **input,
                            const char *chars,
                            int *line,
                            int *column)
{
    size_t count = btp_strspn_location(*input, chars, line, column);
    if (0 == count)
        return count;
    *input += count;
    return count;
}

int
btp_parse_char_span(char **input, const char *accept, char **result)
{
    size_t count = strspn(*input, accept);
    if (count == 0)
        return 0;
    *result = btp_strndup(*input, count);
    *input += count;
    return count;
}

bool
btp_parse_char_cspan(char **input, const char *reject, char **result)
{
    size_t count = strcspn(*input, reject);
    if (count == 0)
        return false;
    *result = btp_strndup(*input, count);
    *input += count;
    return true;
}

int
btp_skip_string(char **input, const char *string)
{
    char *local_input = *input;
    const char *local_string = string;
    while (*local_string && *local_input && *local_input == *local_string)
    {
        ++local_input;
        ++local_string;
    }
    if (*local_string != '\0')
        return 0;
    int count = local_input - *input;
    *input = local_input;
    return count;
}

bool
btp_parse_string(char **input, const char *string, char **result)
{
    char *local_input = *input;
    const char *local_string = string;
    while (*local_string && *local_input && *local_input == *local_string)
    {
        ++local_input;
        ++local_string;
    }
    if (*local_string != '\0')
        return false;
    *result = btp_strndup(string, local_input - *input);
    *input = local_input;
    return true;
}

char
btp_parse_digit(char **input)
{
    char digit = **input;
    if (digit < '0' || digit > '9')
        return '\0';
    ++*input;
    return digit;
}

int
btp_skip_unsigned_integer(char **input)
{
    return btp_skip_char_span(input, "0123456789");
}

int
btp_parse_unsigned_integer(char **input, unsigned *result)
{
    char *local_input = *input;
    char *numstr;
    int length = btp_parse_char_span(&local_input,
                                     "0123456789",
                                     &numstr);
    if (0 == length)
        return 0;

    char *endptr;
    errno = 0;
    unsigned long r = strtoul(numstr, &endptr, 10);
    bool failure = (errno || numstr == endptr || *endptr != '\0'
                    || r > UINT_MAX);
    free(numstr);
    if (failure) /* number too big or some other error */
        return 0;
    *result = r;
    *input = local_input;
    return length;
}

int
btp_skip_hexadecimal_number(char **input)
{
    char *local_input = *input;
    if (!btp_skip_char(&local_input, '0'))
        return 0;
    if (!btp_skip_char(&local_input, 'x'))
        return 0;
    int count = 2;
    count += btp_skip_char_span(&local_input, "abcdef0123456789");
    if (2 == count) /* btp_skip_char_span returned 0 */
        return 0;
    *input = local_input;
    return count;
}

int
btp_parse_hexadecimal_number(char **input, uint64_t *result)
{
    char *local_input = *input;
    if (!btp_skip_char(&local_input, '0'))
        return 0;
    if (!btp_skip_char(&local_input, 'x'))
        return 0;
    int count = 2;
    char *numstr;
    count += btp_parse_char_span(&local_input,
                                 "abcdef0123456789",
                                 &numstr);

    if (2 == count) /* btp_parse_char_span returned 0 */
        return 0;
    char *endptr;
    errno = 0;
    unsigned long long r = strtoull(numstr, &endptr, 16);
    bool failure = (errno || numstr == endptr || *endptr != '\0');
    free(numstr);
    if (failure) /* number too big or some other error */
        return 0;
    *result = r;
    *input = local_input;
    return count;
}
