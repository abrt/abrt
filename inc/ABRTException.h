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
