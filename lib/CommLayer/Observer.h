#ifndef OBSERVER_H_
#define OBSERVER_H_

#include <string>
#include <iostream>
#include <stdint.h>
#include "DBusCommon.h"

class CObserver {
    public:
        virtual ~CObserver() {}
        virtual void Status(const std::string& pMessage, uint64_t pDest=0) = 0;
        virtual void Debug(const std::string& pMessage) = 0;
        virtual void Warning(const std::string& pMessage, uint64_t pDest=0) = 0;
};

#endif
