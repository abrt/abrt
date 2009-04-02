#include "Settings.h"
#include <fstream>
#include <stdlib.h>

#define SECTION_COMMON      "Common"
#define SECTION_REPORTERS   "Reporters"
#define SECTION_ACTIONS     "Actions"

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
                else if (line[ii] == '#')
                {
                    break;
                }
                else if (line[ii] == '[')
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
                else if (line[ii] == '=')
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
            if (!is_key)
            {
                if (section == SECTION_COMMON)
                {
                    m_mapSettingsCommon[key] = value;
                }
                else if (section == SECTION_REPORTERS)
                {
                    m_mapSettingsReporters[key] = value;
                }
                else if (section == SECTION_ACTIONS)
                {
                    m_mapSettingsActions[key] = value;
                }
            }
        }
        fIn.close();
    }
    ParseCommon();
    ParseReporters();
    ParseActions();
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
    if (m_mapSettingsCommon.find("EnabledPlugins") != m_mapSettingsCommon.end())
    {
        m_setEnabledPlugins = ParseList(m_mapSettingsCommon["EnabledPlugins"]);
    }
    if (m_mapSettingsCommon.find("Database") != m_mapSettingsCommon.end())
    {
        m_sDatabase =  m_mapSettingsCommon["Database"];
    }
    if (m_mapSettingsCommon.find("MaxCrashReportsSize") != m_mapSettingsCommon.end())
    {
        m_nMaxCrashReportsSize =  atoi(m_mapSettingsCommon["MaxCrashReportsSize"].c_str());
    }
}

void  CSettings::ParseReporters()
{
    map_settings_t::iterator it;
    for (it = m_mapSettingsReporters.begin(); it != m_mapSettingsReporters.end(); it++)
    {
        m_mapAnalyzerReporters[it->first] = ParseList(it->second);
    }
}

void CSettings::ParseActions()
{
    map_settings_t::iterator it;
    for (it = m_mapSettingsActions.begin(); it != m_mapSettingsActions.end(); it++)
    {
        set_strings_t keys = ParseActionKey(it->first);
        set_actions_t singleActions = ParseActionValue(it->second);
        set_strings_t::iterator it_keys;
        for (it_keys = keys.begin(); it_keys != keys.end(); it_keys++)
        {
            m_mapAnalyzerActions[*it_keys] = singleActions;
        }
    }
}


CSettings::set_actions_t CSettings::ParseActionValue(const std::string& pValue)
{
    set_actions_t singleActions;
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
            singleActions.insert(make_pair(action, item));
            item = "";
            is_arg = false;
            action = "";
        }
        else if (pValue[ii] == ',' && !is_quote && !is_arg)
        {
            if (item != "")
            {
                singleActions.insert(make_pair(item, ""));
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
        singleActions.insert(make_pair(item, ""));
    }
    return singleActions;
}

CSettings::set_strings_t CSettings::ParseActionKey(const std::string& Key)
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
       else if (Key[ii] == '(' && !is_quote)
       {
           key = item;
           item = "";
       }
       else if ((Key[ii] == ',' || Key[ii] == ')') && !is_quote)
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
        set.insert(item);
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

const bool& CSettings::GetOpenGPGCheck()
{
    return m_bOpenGPGCheck;
}

const CSettings::map_analyzer_reporters_t& CSettings::GetReporters()
{
    return m_mapAnalyzerReporters;
}

const CSettings::map_analyzer_actions_t& CSettings::GetActions()
{
    return m_mapAnalyzerActions;
}
const unsigned int& CSettings::GetMaxCrashReportsSize()
{
    return m_nMaxCrashReportsSize;
}

const std::string& CSettings::GetDatabase()
{
    return m_sDatabase;
}
