#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>

#undef NDEBUG
#define DEBUG
#include <assert.h>


int main(int argc, char *argv[])
{
    for (int level = LOG_DEBUG; level != LOG_EMERG; --level)
        syslog(level, "%s is on its way to die ... %d", argv[0], level);

    assert(!"See you in the hell!");

    /* Muhehe ... */
    return 0;
}
