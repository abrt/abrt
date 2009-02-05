
#include "Settings.h"
#include <fstream>


void load_settings(const std::string& path, map_settings_t& settings)
{
    std::ifstream fIn;
    fIn.open(path.c_str());
    if (fIn.is_open())
    {
        std::string line;
        while (!fIn.eof())
        {
            getline(fIn, line);

            int ii;
            bool is_value = false;
            bool valid = false;
            std::string key = "";
            std::string value = "";
            for (ii = 0; ii < line.length(); ii++)
            {
                if (isspace(line[ii]))
                {
                    continue;
                }
                if (line[ii] == '#')
                {
                    break;
                }
                else if (line[ii] == '=')
                {
                    is_value = true;
                }
                else if (line[ii] == '=' && is_value)
                {
                    key = "";
                    value = "";
                    break;
                }
                else if (!is_value)
                {
                    key += line[ii];
                }
                else
                {
                    valid = true;
                    value += line[ii];
                }
            }
            if (valid)
            {
                settings[key] = value;
            }
        }
        fIn.close();
    }
}

void save_settings(const std::string& path, const map_settings_t& settings)
{
    //TODO: write this
}
