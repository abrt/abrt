#ifndef OBSERVER_H_
#define OBSERVER_H_

#include <string>
#include <stdint.h>
#include "DBusCommon.h"

class CObserver {
    public:
        virtual ~CObserver() {}
        virtual void Status(const std::string& pMessage, const char* peer, uint64_t pDest) = 0;
        virtual void Warning(const std::string& pMessage, const char* peer, uint64_t pDest) = 0;
};

#endif
