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
#ifndef ABRTEXCEPTION_H_
#define ABRTEXCEPTION_H_

#include "abrtlib.h"

typedef enum {
    EXCEP_UNKNOW,
    EXCEP_DD_OPEN,
    EXCEP_DD_LOAD,
    EXCEP_DD_SAVE,
    EXCEP_DD_DELETE,
    EXCEP_DL,
    EXCEP_PLUGIN,
    EXCEP_ERROR,
} abrt_exception_t;

/* std::exception is a class with virtual members.
 * deriving from it makes our ctor/dtor much more heavy,
 * and those are inlined in every throw and catch site!
 */
class CABRTException /*: public std::exception*/
{
    private:
        abrt_exception_t m_type;
        char *m_what;

        /* Not defined. You can't use it */
        CABRTException& operator= (const CABRTException&);

    public:
        ~CABRTException() { free(m_what); }
        CABRTException(abrt_exception_t type, const char* fmt, ...);
        CABRTException(const CABRTException& rhs);

        abrt_exception_t type() { return m_type; }
        const char* what() const { return m_what; }
};

#endif
