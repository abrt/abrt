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
