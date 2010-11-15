/*
    location.h

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
#ifndef BTPARSER_LOCATION_H
#define BTPARSER_LOCATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A location in the backtrace file with an attached message.
 * It's used for error reporting: the line and the column points to
 * the place where a parser error occurred, and the message explains
 * what the parser expected and didn't find on that place.
 */
struct btp_location
{
    /** Starts from 1. */
    int line;
    /** Starts from 0. */
    int column;
    /**
     * Error message related to the line and column.  Do not release
     * the memory this pointer points to.
     */
    const char *message;
};

/**
 * Initializes all members of the location struct to their default
 * values.  No memory is allocated or released by this function.
 */
void
btp_location_init(struct btp_location *location);

/**
 * Adds a line and a column to specific location.
 * @note
 * If the line is not 1 (meaning the first line), the column in the
 * location structure is overwritten by the provided add_column value.
 * Otherwise the add_column value is added to the column member of the
 * location structure.
 * @param location
 * The structure to be modified. It must be a valid pointer.
 * @param add_line
 * Starts from 1. It means that if add_line is 1, the line member of the
 * location structure is not changed.
 * @param add_column
 * Starts from 0.
 */
void
btp_location_add(struct btp_location *location,
                 int add_line,
                 int add_column);

/**
 * Adds a line column pair to another line column pair.
 * @note
 * If the add_line is not 1 (meaning the frist line), the column is
 * overwritten by the provided add_column value.  Otherwise the
 * add_column value is added to the column.
 * @param add_line
 * Starts from 1. It means that if add_line is 1, the line is not
 * changed.
 * @param add_column
 * Starts from 0.
 */
void
btp_location_add_ext(int *line,
                     int *column,
                     int add_line,
                     int add_column);

/**
 * Updates the line and column of the location by moving "after" the
 * char c. If c is a newline character, the line number is increased
 * and the column is set to 0. Otherwise the column is increased by 1.
 */
void
btp_location_eat_char(struct btp_location *location,
                      char c);

/**
 * Updates the line and the column by moving "after" the char c. If c
 * is a newline character, the line number is increased and the column
 * is set to 0. Otherwise the column is increased.
 * @param line
 * Must be a valid pointer.
 * @param column
 * Must be a valid pointer.
 */
void
btp_location_eat_char_ext(int *line,
                          int *column,
                          char c);

#ifdef __cplusplus
}
#endif

#endif
