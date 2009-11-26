#include "abrtlib.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

using namespace std;

void parse_release(const char *pRelease, string& pProduct, string& pVersion)
{
    if (strstr(pRelease, "Rawhide"))
    {
        pProduct = "Fedora";
        pVersion = "rawhide";
        VERB3 log("%s:Version is '%s' and product is '%s'",__func__, pVersion.c_str(), pProduct.c_str());
        return;
    }
    if (strstr(pRelease, "Fedora"))
    {
        pProduct = "Fedora";
    }
    else if (strstr(pRelease, "Red Hat Enterprise Linux"))
    {
        pProduct = "Red Hat Enterprise Linux ";
    }

    const char *release = strstr(pRelease, "release");
    const char *space = release ? strchr(release, ' ') : NULL;

    if (space++) while (*space != '\0' && *space != ' ')
    {
        /* Eat string like "5.2" */
        pVersion += *space;
        if (pProduct == "Red Hat Enterprise Linux ")
        {
            pProduct += *space;
        }
        space++;
    }
    VERB3 log("%s:Version is '%s' and product is '%s'",__func__, pVersion.c_str(), pProduct.c_str());
}
