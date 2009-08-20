#include "Settings.h"
#include <fstream>
#include <stdlib.h>

#define SECTION_COMMON      "Common"
#define SECTION_ANALYZER_ACTIONS_AND_REPORTERS   "AnalyzerActionsAndReporters"
#define SECTION_CRON        "Cron"

CSettings::CSettings() :
    m_bOpenGPGCheck(false),
    m_nMaxCrashReportsSize(1000)
{}

void CSettings::LoadSettings(const std::string& pPath)
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
                else if (line[ii] == '#' && !is_quote)
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
                    if (m_mapSettingsCommon[key] != "")
                    {
                        m_mapSettingsCommon[key] += ",";
                    }
                    m_mapSettingsCommon[key] += value;
                }
                else if (section == SECTION_ANALYZER_ACTIONS_AND_REPORTERS)
                {
                    if (m_mapSettingsAnalyzerActionsAndReporters[key] != "")
                    {
                        m_mapSettingsAnalyzerActionsAndReporters[key] += ",";
                    }
                    m_mapSettingsAnalyzerActionsAndReporters[key] += value;
                }
                else if (section == SECTION_CRON)
                {
                    if (m_mapSettingsCron[key] != "")
                    {
                        m_mapSettingsCron[key] += ",";
                    }
                    m_mapSettingsCron[key] += value;
                }
            }
        }
        fIn.close();
    }
    ParseCommon();
    ParseAnalyzerActionsAndReporters();
    ParseCron();
}

void CSettings::ParseCommon()
{
    if (m_mapSettingsCommon.find("OpenGPGCheck") != m_mapSettingsCommon.end())
    {
        m_bOpenGPGCheck = m_mapSettingsCommon["OpenGPGCheck"] == "yes";
    }
    if (m_mapSettingsCommon.find("OpenGPGPublicKeys") != m_mapSettingsCommon.end())
    {
        m_setOpenGPGPublicKeys = ParseList(m_mapSettingsCommon["OpenGPGPublicKeys"]);
    }
    if (m_mapSettingsCommon.find("BlackList") != m_mapSettingsCommon.end())
    {
        m_setBlackList = ParseList(m_mapSettingsCommon["BlackList"]);
    }
    if (m_mapSettingsCommon.find("Database") != m_mapSettingsCommon.end())
    {
        m_sDatabase =m_mapSettingsCommon["Database"];
    }
    if (m_mapSettingsCommon.find("EnabledPlugins") != m_mapSettingsCommon.end())
    {
        m_setEnabledPlugins = ParseList(m_mapSettingsCommon["EnabledPlugins"]);
    }
    if (m_mapSettingsCommon.find("MaxCrashReportsSize") != m_mapSettingsCommon.end())
    {
        m_nMaxCrashReportsSize =  atoi(m_mapSettingsCommon["MaxCrashReportsSize"].c_str());
    }
    if (m_mapSettingsCommon.find("ActionsAndReporters") != m_mapSettingsCommon.end())
    {
        m_vectorActionsAndReporters =  ParseListWithArgs(m_mapSettingsCommon["ActionsAndReporters"]);
    }
}

void CSettings::ParseAnalyzerActionsAndReporters()
{
    map_settings_t::iterator it;
    for (it = m_mapSettingsAnalyzerActionsAndReporters.begin(); it != m_mapSettingsAnalyzerActionsAndReporters.end(); it++)
    {
        set_strings_t keys = ParseKey(it->first);
        vector_pair_strings_t actionsAndReporters = ParseListWithArgs(it->second);
        set_strings_t::iterator it_keys;
        for (it_keys = keys.begin(); it_keys != keys.end(); it_keys++)
        {
            m_mapAnalyzerActionsAndReporters[*it_keys] = actionsAndReporters;
        }
    }
}

void CSettings::ParseCron()
{
    map_settings_t::iterator it;
    for (it = m_mapSettingsCron.begin(); it != m_mapSettingsCron.end(); it++)
    {
        vector_pair_strings_t actionsAndReporters = ParseListWithArgs(it->second);
    m_mapCron[it->first] = actionsAndReporters;
    }
}

CSettings::vector_pair_strings_t CSettings::ParseListWithArgs(const std::string& pValue)
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

CSettings::set_strings_t CSettings::ParseKey(const std::string& Key)
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

CSettings::set_strings_t CSettings::ParseList(const std::string& pList)
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


const CSettings::set_strings_t& CSettings::GetBlackList()
{
    return m_setBlackList;
}

const CSettings::set_strings_t& CSettings::GetEnabledPlugins()
{
    return m_setEnabledPlugins;
}

const CSettings::set_strings_t& CSettings::GetOpenGPGPublicKeys()
{
    return m_setOpenGPGPublicKeys;
}

bool CSettings::GetOpenGPGCheck()
{
    return m_bOpenGPGCheck;
}

const CSettings::map_analyzer_actions_and_reporters_t& CSettings::GetAnalyzerActionsAndReporters()
{
    return m_mapAnalyzerActionsAndReporters;
}

const unsigned int& CSettings::GetMaxCrashReportsSize()
{
    return m_nMaxCrashReportsSize;
}

const CSettings::vector_pair_strings_t& CSettings::GetActionsAndReporters()
{
    return m_vectorActionsAndReporters;
}

const std::string& CSettings::GetDatabase()
{
    return m_sDatabase;
}

const CSettings::map_cron_t& CSettings::GetCron()
{
    return m_mapCron;
}
