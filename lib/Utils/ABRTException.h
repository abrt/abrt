#ifndef ABRTEXCEPTION_H_
#define ABRTEXCEPTION_H_

#include <string>

typedef enum {EXCEP_UNKNOW,
              EXCEP_DD_OPEN,
              EXCEP_DD_LOAD,
              EXCEP_DD_SAVE,
              EXCEP_DD_DELETE,
              EXCEP_DL,
              EXCEP_PLUGIN,
              EXCEP_ERROR,
              EXCEP_FATAL} abrt_exception_t;

class CABRTException
{
    private:
        std::string m_sWhat;
        abrt_exception_t m_Type;
    public:
        CABRTException(const abrt_exception_t& pType, const char* pWhat) :
            m_sWhat(pWhat),
            m_Type(pType)
        {}
        CABRTException(const abrt_exception_t& pType, const std::string& pWhat) :
            m_sWhat(pWhat),
            m_Type(pType)
        {}
        abrt_exception_t type() { return m_Type; }
        std::string what() { return m_sWhat; }
};

#endif /* ABRTEXCEPTION_H_ */
