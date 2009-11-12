#include "abrtlib.h"

void parse_args(const char *psArgs, vector_string_t& pArgs, const char quote)
{
    unsigned ii;
    bool is_quote = false;
    std::string item;

    for (ii = 0; psArgs[ii]; ii++)
    {
        if (quote != -1 && psArgs[ii] == quote)
        {
                is_quote = !is_quote;
        }
        else if (psArgs[ii] == ',' && !is_quote)
        {
            pArgs.push_back(item);
            item.clear();
        }
        else
        {
            item += psArgs[ii];
        }
    }

    if (item.size() != 0)
    {
        pArgs.push_back(item);
    }
}
