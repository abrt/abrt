#include "abrtlib.h"

void parse_args(const char *psArgs, vector_string_t& pArgs, int quote)
{
    unsigned ii;
    bool inside_quotes = false;
    std::string item;

    for (ii = 0; psArgs[ii]; ii++)
    {
        if (quote != -1)
        {
            if (psArgs[ii] == quote)
            {
                inside_quotes = !inside_quotes;
                continue;
            }
            /* inside quotes we support escaping with \x */
            if (inside_quotes && psArgs[ii] == '\\' && psArgs[ii+1])
            {
                ii++;
                item += psArgs[ii];
                continue;
            }
        }
        if (psArgs[ii] == ',' && !inside_quotes)
        {
            pArgs.push_back(item);
            item.clear();
            continue;
        }
        item += psArgs[ii];
    }

    if (item.size() != 0)
    {
        pArgs.push_back(item);
    }
}
