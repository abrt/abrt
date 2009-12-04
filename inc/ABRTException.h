#ifndef ABRTEXCEPTION_H_
#define ABRTEXCEPTION_H_

#include <string>

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
        std::string m_sWhat;
        abrt_exception_t m_Type;

    public:
        /* virtual ~CABRTException() throw() {} */
        CABRTException(abrt_exception_t pType, const char* pWhat) :
            m_sWhat(pWhat),
            m_Type(pType)
        {}
        CABRTException(abrt_exception_t pType, const std::string& pWhat) :
            m_sWhat(pWhat),
            m_Type(pType)
        {}
        abrt_exception_t type() { return m_Type; }
        const char* what() const { return m_sWhat.c_str(); }
};

#endif
