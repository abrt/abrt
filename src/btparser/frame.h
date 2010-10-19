/*
    frame.h

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
#ifndef BTPARSER_FRAME_H
#define BTPARSER_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct strbuf;
struct btp_location;

/**
 * A frame representing a function call or a signal handler on a call
 * stack of a thread.
 */
struct btp_frame
{
    /**
     * A function name or NULL. If it's NULL, signal_handler_called is
     * true.
     */
    char *function_name;
    /**
     * A function type, or NULL if it isn't present.
     */
    char *function_type;
    /**
     * A frame number in a thread. It does not necessarily show the
     * actual position in the thread, as this number is set by the
     * parser and never updated.
     */
    unsigned number;
    /**
     * The name of the source file containing the function definition,
     * or the name of the binary file (.so) with the binary code of
     * the function, or NULL.
     */
    char *source_file;
    /**
     * A line number in the source file, determining the position of
     * the function definition, or -1 when unknown.
     */
    unsigned source_line;
    /**
     * Signal handler was called on this frame.
     */
    bool signal_handler_called;
    /**
     * The function address in the computer memory, or -1 when the
     * address is unknown.
     */
    uint64_t address;
    /**
     * A sibling frame residing below this one, or NULL if this is the
     * last frame in the parent thread.
     */
    struct btp_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_frame_free().
 */
struct btp_frame *
btp_frame_new();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
btp_frame_init(struct btp_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
btp_frame_free(struct btp_frame *frame);

/**
 * Creates a duplicate of the frame.
 * @param frame
 * It must be non-NULL pointer. The frame is not modified by calling
 * this function.
 * @param siblings
 * Whether to duplicate also siblings referenced by frame->next.  If
 * false, frame->next is not duplicated for the new frame, but it is
 * set to NULL.
 * @returns
 * This function never returns NULL. If the returned duplicate is not
 * shallow, it must be released by calling the function
 * btp_frame_free().
 */
struct btp_frame *
btp_frame_dup(struct btp_frame *frame,
              bool siblings);

/**
 * Checks whether the frame represents a call of function with certain
 * function name.
 */
bool
btp_frame_calls_func(struct btp_frame *frame,
                     const char *function_name);

/**
 * Checks whether the frame represents a call of function with certain
 * function name, which resides in a source file.
 * @param source_file
 * The frame's source_file is searched for the source_file as a
 * substring.
 */
bool
btp_frame_calls_func_in_file(struct btp_frame *frame,
                             const char *function_name,
                             const char *source_file);

/**
 * Checks whether the frame represents a call of function with certain
 * function name, which resides in one of the source files.
 * @param source_file0
 * The frame's source_file is searched for the source_file0 as a
 * substring.
 * @returns
 * True if the frame corresponds to a function with function_name,
 * residing in the source_file0, or source_file1.
 */
bool
btp_frame_calls_func_in_file2(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1);

/**
 * Checks whether the frame represents a call of function with certain
 * function name, which resides in one of the source files.
 * @param source_file0
 * The frame's source_file is searched for the source_file0 as a
 * substring.
 * @returns
 * True if the frame corresponds to a function with function_name,
 * residing in the source_file0, source_file1, or source_file2.
 */
bool
btp_frame_calls_func_in_file3(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1,
                              const char *source_file2);

/**
 * Checks whether the frame represents a call of function with certain
 * function name, which resides in one of the source files.
 * @param source_file0
 * The frame's source_file is searched for the source_file0 as a
 * substring.
 * @returns
 * True if the frame corresponds to a function with function_name,
 * residing in the source_file0, source_file1, source_file2, or
 * source_file3.
 */
bool
btp_frame_calls_func_in_file4(struct btp_frame *frame,
                              const char *function_name,
                              const char *source_file0,
                              const char *source_file1,
                              const char *source_file2,
                              const char *source_file3);

/**
 * Compares two frames.
 * @param f1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param f2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param compare_number
 * Indicates whether to include the frame numbers in the
 * comparsion. If set to false, the frame numbers are ignored.
 * @returns
 * Returns 0 if the frames are same.  Returns negative number if f1 is
 * found to be 'less' than f2.  Returns positive number if f1 is found
 * to be 'greater' than f2.
 */
int
btp_frame_cmp(struct btp_frame *f1,
              struct btp_frame *f2,
              bool compare_number);

/**
 * Puts the frame 'b' to the bottom of the stack 'a'. In other words,
 * it finds the last sibling of the frame 'a', and appends the frame
 * 'b' to this last sibling.
 */
void
btp_frame_add_sibling(struct btp_frame *a,
                      struct btp_frame *b);

/**
 * Appends the textual representation of the frame to the string
 * buffer.
 * @param frame
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 */
void
btp_frame_append_to_str(struct btp_frame *frame,
                        struct strbuf *str,
                        bool verbose);

/**
 * If the input contains a complete frame, this function parses the
 * frame text, returns it in a structure, and moves the input pointer
 * after the frame.  If the input does not contain proper, complete
 * frame, the function does not modify input and returns NULL.
 * @returns
 * Allocated pointer with a frame structure. The pointer should be
 * released by btp_frame_free().
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  When this function returns NULL, the structure will contain
 * the error line, column, and message.  The line and column members
 * of the location are gradually increased as the parser handles the
 * input, so the location should be initialized before calling this
 * function to get reasonable values.
 */
struct btp_frame *
btp_frame_parse(char **input,
                struct btp_location *location);

/**
 * If the input contains a proper frame start section, parse the frame
 * number, and move the input pointer after this section. Otherwise do
 * not modify input.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a frame start.
 * @code
 * "#1 "
 * "#255  "
 * @endcode
 */
int
btp_frame_parse_frame_start(char **input, unsigned *number);

/**
 * Parses C++ operator on input.
 * Supports even 'operator new[]' and 'operator delete[]'.
 * @param target
 * The parsed operator name is appened to the string buffer provided,
 * if an operator is found. Otherwise the string buffer is not
 * changed.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain operator.
 */
int
btp_frame_parseadd_operator(char **input,
                            struct strbuf *target);

/**
 * Parses a part of function name from the input.
 * @param target
 * Pointer to a non-allocated pointer. This function will set
 * the pointer to newly allocated memory containing the name chunk,
 * if it returns positive, nonzero value.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a part of function name.
 */
int
btp_frame_parse_function_name_chunk(char **input,
                                    bool space_allowed,
                                    char **target);

/**
 * If the input buffer contains part of function name containing braces,
 * for example "(anonymous namespace)", parse it, append the contents
 * to target and move input after the braces.
 * Otherwise do not modify niether the input nor the target.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a braced part of function name.
 */
int
btp_frame_parse_function_name_braces(char **input,
                                     char **target);

/**
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a template part of function name.
 */
int
btp_frame_parse_function_name_template(char **input,
                                       char **target);

/**
 * Parses the function name, which is a part of the frame header, from
 * the input. If the frame header contains also the function type,
 * it's also parsed.
 * @param function_name
 * A pointer pointing to an uninitialized pointer. This function
 * allocates a string and sets the pointer to it if it parses the
 * function name from the input successfully.  The memory returned
 * this way must be released by the caller using the function free().
 * If this function returns true, this pointer is guaranteed to be
 * non-NULL.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 * @returns
 * True if the input stream contained a function name, which has been
 * parsed. False otherwise.
 */
bool
btp_frame_parse_function_name(char **input,
                              char **function_name,
                              char **function_type,
                              struct btp_location *location);

/**
 * Skips function arguments which are a part of the frame header, in
 * the input stream.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
btp_frame_skip_function_args(char **input,
                             struct btp_location *location);

/**
 * If the input contains proper function call, parse the function
 * name and store it to result, move the input pointer after whole
 * function call, and return true. Otherwise do not modify the input
 * and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the the function_name.
 * @todo
 * Parse and return the function call arguments.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
btp_frame_parse_function_call(char **input,
                              char **function_name,
                              char **function_type,
                              struct btp_location *location);

/**
 * If the input contains address and function call, parse them, move
 * the input pointer after this sequence, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the parameter function.
 *
 * @code
 * 0x000000322160e7fd in fsync ()
 * 0x000000322222987a in write_to_temp_file (
 * filename=0x18971b0 "/home/jfclere/.recently-used.xbel",
 * contents=<value optimized out>, length=29917, error=0x7fff3cbe4110)
 * @endcode
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
btp_frame_parse_address_in_function(char **input,
                                    uint64_t *address,
                                    char **function_name,
                                    char **function_type,
                                    struct btp_location *location);

/**
 * If the input contains sequence "from path/to/file:fileline" or "at
 * path/to/file:fileline", parse it, move the input pointer after this
 * sequence and return true. Otherwise do not modify the input and
 * return false.
 *
 * The ':' followed by line number is optional. If it is not present,
 * the fileline is set to -1.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
btp_frame_parse_file_location(char **input,
                              char **file,
                              unsigned *fileline,
                              struct btp_location *location);

/**
 * If the input contains proper frame header, this function
 * parses the frame header text, moves the input pointer
 * after the frame header, and returns a frame struct.
 * If the input does not contain proper frame header, this function
 * returns NULL and does not modify input.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 * @returns
 * Newly created frame struct or NULL. The returned frame struct
 * should be released by btp_frame_free().
 */
struct btp_frame *
btp_frame_parse_header(char **input,
                       struct btp_location *location);

#ifdef __cplusplus
}
#endif

#endif
