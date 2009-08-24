#include "Settings.h"
#include <fstream>
#include <stdlib.h>

#define SECTION_COMMON      "Common"
#define SECTION_ANALYZER_ACTIONS_AND_REPORTERS   "AnalyzerActionsAndReporters"
#define SECTION_CRON        "Cron"


set_strings_t g_settings_setOpenGPGPublicKeys;
set_strings_t g_settings_mapSettingsBlackList;
vector_pair_string_string_t g_settings_vectorActionsAndReporters;
set_strings_t g_settings_setEnabledPlugins;
map_analyzer_actions_and_reporters_t g_settings_mapAnalyzerActionsAndReporters;
unsigned int g_settings_nMaxCrashReportsSize = 1000;
bool g_settings_bOpenGPGCheck = false;
std::string g_settings_sDatabase;
map_cron_t g_settings_mapCron;

static map_settings_t s_mapSettingsCommon;
static map_settings_t s_mapSettingsAnalyzerActionsAndReporters;
static map_settings_t s_mapSettingsCron;


static set_strings_t ParseList(const std::string& pList)
{
   unsigned int ii;
   std::string item  = "";
   set_strings_t set;
   for(ii = 0; ii < pList.size(); ii++)
   {
       if (pList[ii] == ',')
       {
           set.insert(item);
           item = "";
       }
       else
       {
          item += pList[ii];
       }
   }
   if (item != "")
   {
       set.insert(item);
   }
   return set;
}

static vector_pair_strings_t ParseListWithArgs(const std::string& pValue)
{
    vector_pair_strings_t pluginsWithArgs;
    unsigned int ii;
    std::string item = "";
    std::string action = "";
    bool is_quote = false;
    bool is_arg = false;
    for (ii = 0; ii < pValue.size(); ii++)
    {
        if (pValue[ii] == '\"')
        {
            is_quote = is_quote == true ? false : true;
            item += pValue[ii];
        }
        else if (pValue[ii] == '(' && !is_quote)
        {
            action = item;
            item = "";
            is_arg = true;
        }
        else if (pValue[ii] == ')' && is_arg && !is_quote)
        {
            pluginsWithArgs.push_back(make_pair(action, item));
            item = "";
            is_arg = false;
            action = "";
        }
        else if (pValue[ii] == ',' && !is_quote && !is_arg)
        {
            if (item != "")
            {
                pluginsWithArgs.push_back(make_pair(item, ""));
                item = "";
            }
        }
        else
        {
            item += pValue[ii];
        }
    }
    if (item != "")
    {
        pluginsWithArgs.push_back(make_pair(item, ""));
    }
    return pluginsWithArgs;
}

static void ParseCommon()
{
    if (s_mapSettingsCommon.find("OpenGPGCheck") != s_mapSettingsCommon.end())
    {
        g_settings_bOpenGPGCheck = s_mapSettingsCommon["OpenGPGCheck"] == "yes";
    }
    if (s_mapSettingsCommon.find("OpenGPGPublicKeys") != s_mapSettingsCommon.end())
    {
        g_settings_setOpenGPGPublicKeys = ParseList(s_mapSettingsCommon["OpenGPGPublicKeys"]);
    }
    if (s_mapSettingsCommon.find("BlackList") != s_mapSettingsCommon.end())
    {
        g_settings_mapSettingsBlackList = ParseList(s_mapSettingsCommon["BlackList"]);
    }
    if (s_mapSettingsCommon.find("Database") != s_mapSettingsCommon.end())
    {
        g_settings_sDatabase = s_mapSettingsCommon["Database"];
    }
    if (s_mapSettingsCommon.find("EnabledPlugins") != s_mapSettingsCommon.end())
    {
        g_settings_setEnabledPlugins = ParseList(s_mapSettingsCommon["EnabledPlugins"]);
    }
    if (s_mapSettingsCommon.find("MaxCrashReportsSize") != s_mapSettingsCommon.end())
    {
        g_settings_nMaxCrashReportsSize = atoi(s_mapSettingsCommon["MaxCrashReportsSize"].c_str());
    }
    if (s_mapSettingsCommon.find("ActionsAndReporters") != s_mapSettingsCommon.end())
    {
        g_settings_vectorActionsAndReporters = ParseListWithArgs(s_mapSettingsCommon["ActionsAndReporters"]);
    }
}

static void ParseCron()
{
    map_settings_t::iterator it;
    for (it = s_mapSettingsCron.begin(); it != s_mapSettingsCron.end(); it++)
    {
        vector_pair_strings_t actionsAndReporters = ParseListWithArgs(it->second);
        g_settings_mapCron[it->first] = actionsAndReporters;
    }
}

static set_strings_t ParseKey(const std::string& Key)
{
   unsigned int ii;
   std::string item  = "";
   std::string key = "";
   set_strings_t set;
   bool is_quote = false;
   for(ii = 0; ii < Key.size(); ii++)
   {
       if (Key[ii] == '\"')
       {
          is_quote = is_quote == true ? false : true;
       }
       else if (Key[ii] == ':' && !is_quote)
       {
           key = item;
           item = "";
       }
       else if ((Key[ii] == ',') && !is_quote)
       {
           set.insert(key + ":" + item);
           item = "";
       }
       else
       {
           item += Key[ii];
       }
    }
    if (item != "" && !is_quote)
    {
        if (key == "")
        {
            set.insert(item);
        }
        else
        {
            set.insert(key + ":" + item);
        }
    }
    return set;
}

static void ParseAnalyzerActionsAndReporters()
{
    map_settings_t::iterator it;
    for (it = s_mapSettingsAnalyzerActionsAndReporters.begin(); it != s_mapSettingsAnalyzerActionsAndReporters.end(); it++)
    {
        set_strings_t keys = ParseKey(it->first);
        vector_pair_strings_t actionsAndReporters = ParseListWithArgs(it->second);
        set_strings_t::iterator it_keys;
        for (it_keys = keys.begin(); it_keys != keys.end(); it_keys++)
        {
            g_settings_mapAnalyzerActionsAndReporters[*it_keys] = actionsAndReporters;
        }
    }
}

void LoadSettings(const std::string& pPath)
{
    std::ifstream fIn;
    fIn.open(pPath.c_str());
    if (fIn.is_open())
    {
        std::string line;
        std::string section = "";
        while (!fIn.eof())
        {
            getline(fIn, line);

            unsigned int ii;
            bool is_key = true;
            bool is_section = false;
            bool is_quote = false;
            std::string key = "";
            std::string value = "";
            for (ii = 0; ii < line.length(); ii++)
            {
                if (isspace(line[ii]) && !is_quote)
                {
                    continue;
                }
                else if (line[ii] == '#' && !is_quote && key == "")
                {
                    break;
                }
                else if (line[ii] == '[' && !is_quote)
                {
                    is_section = true;
                    section = "";
                }
                else if (line[ii] == '\"')
                {
                    is_quote = is_quote == true ? false : true;
                    value += line[ii];
                }
                else if (is_section)
                {
                    if (line[ii] == ']')
                    {
                        break;
                    }
                    section += line[ii];
                }
                else if (line[ii] == '=' && !is_quote)
                {
                    is_key = false;
                    key = value;
                    value = "";
                }
                else
                {
                    value += line[ii];
                }
            }
            if (!is_key && !is_quote)
            {
                if (section == SECTION_COMMON)
                {
                    if (s_mapSettingsCommon[key] != "")
                    {
                        s_mapSettingsCommon[key] += ",";
                    }
                    s_mapSettingsCommon[key] += value;
                }
                else if (section == SECTION_ANALYZER_ACTIONS_AND_REPORTERS)
                {
                    if (s_mapSettingsAnalyzerActionsAndReporters[key] != "")
                    {
                        s_mapSettingsAnalyzerActionsAndReporters[key] += ",";
                    }
                    s_mapSettingsAnalyzerActionsAndReporters[key] += value;
                }
                else if (section == SECTION_CRON)
                {
                    if (s_mapSettingsCron[key] != "")
                    {
                        s_mapSettingsCron[key] += ",";
                    }
                    s_mapSettingsCron[key] += value;
                }
            }
        }
        fIn.close();
    }
    ParseCommon();
    ParseAnalyzerActionsAndReporters();
    ParseCron();
}

