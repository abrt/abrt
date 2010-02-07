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
#include "ABRTException.h"

CABRTException::CABRTException(abrt_exception_t type, const char* fmt, ...)
{
    m_type = type;
    va_list ap;
    va_start(ap, fmt);
    m_what = xvasprintf(fmt, ap);
    va_end(ap);
}

CABRTException::CABRTException(const CABRTException& rhs):
    m_type(rhs.m_type),
    m_what(xstrdup(rhs.m_what))
{}
