#include "abrtlib.h"


/* cuts the name from the NVR format: foo-1.2.3-1.el6
   returns a newly allocated string
*/
char* get_package_name_from_NVR_or_NULL(const char* packageNVR)
{
    char* package_name = NULL;
    if(packageNVR != NULL)
    {
        VERB1 log("packageNVR %s", packageNVR);
        package_name = xstrdup(packageNVR);
        char *pos = strrchr(package_name, '-');
        if(pos != NULL)
        {
            *pos = 0;
            pos = strrchr(package_name, '-');
            if(pos != NULL)
            {
                *pos = 0;
            }
        }
    }
    return package_name;
}
