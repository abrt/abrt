/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "Settings.h"
#include "abrtlib.h"
#include "abrt_types.h"
#include "Polkit.h"

#define SECTION_COMMON      "Common"
#define SECTION_ANALYZER_ACTIONS_AND_REPORTERS   "AnalyzerActionsAndReporters"
#define SECTION_CRON        "Cron"

/* Conf file has this format:
 * [ section_name1 ]
 * name1 = value1
 * name2 = value2
 * [ section_name2 ]
 * name = value
 */

/* Static data */
/* Filled by LoadSettings() */

/* map["name"] = "value" strings from [ Common ] section.
 * If the same name found on more than one line,
 * the values are appended, separated by comma: map["name"] = "value1,value2" */
static map_string_t s_mapSectionCommon;
/* ... from [ AnalyzerActionsAndReporters ] */
static map_string_t s_mapSectionAnalyzerActionsAndReporters;
/* ... from [ Cron ] */
static map_string_t s_mapSectionCron;

/* Public data */
/* Written out exactly in this order by SaveSettings() */

/* [ Common ] */
/* one line: "OpenGPGCheck = value" */
bool          g_settings_bOpenGPGCheck = false;
/* one line: "OpenGPGPublicKeys = value1,value2" */
set_string_t  g_settings_setOpenGPGPublicKeys;
set_string_t  g_settings_mapBlackList;
std::string   g_settings_sDatabase;
unsigned int  g_settings_nMaxCrashReportsSize = 1000;
bool          g_settings_bProcessUnpackaged = false;

/* one line: "ActionsAndReporters = aa_first,bb_first(bb_second),cc_first" */
vector_pair_string_string_t g_settings_vectorActionsAndReporters;
/* [ AnalyzerActionsAndReporters ] */
/* many lines, one per key: "map_key = aa_first,bb_first(bb_second),cc_first" */
map_analyzer_actions_and_reporters_t g_settings_mapAnalyzerActionsAndReporters;
/* [ Cron ] */
/* many lines, one per key: "map_key = aa_first,bb_first(bb_second),cc_first" */
map_cron_t    g_settings_mapCron;


/*
 * Loading
 */

static set_string_t ParseList(const char* pList)
{
    unsigned ii;
    std::string item;
    set_string_t set;
    for (ii = 0; pList[ii]; ii++)
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

/* Format: name, name(param),name("param with spaces \"and quotes\"") */
static vector_pair_string_string_t ParseListWithArgs(const char *pValue)
{
    VERB3 log(" ParseListWithArgs(%s)", pValue);

    vector_pair_string_string_t pluginsWithArgs;
    unsigned int ii;
    std::string item;
    std::string action;
    bool is_quote = false;
    bool is_arg = false;
    for (ii = 0; pValue[ii]; ii++)
    {
        if (is_quote && pValue[ii] == '\\' && pValue[ii+1])
        {
            ii++;
            item += pValue[ii];
            continue;
        }
        if (pValue[ii] == '"')
        {
            is_quote = !is_quote;
            /*item += pValue[ii]; - wrong! name("param") must be == name(param) */
            continue;
        }
        if (is_quote)
        {
            item += pValue[ii];
            continue;
        }
        if (pValue[ii] == '(')
        {
            action = item;
            item = "";
            is_arg = true;
            continue;
        }
        if (pValue[ii] == ')' && is_arg)
        {
            VERB3 log(" adding (%s,%s)", action.c_str(), item.c_str());
            pluginsWithArgs.push_back(make_pair(action, item));
            item = "";
            is_arg = false;
            action = "";
            continue;
        }
        if (pValue[ii] == ',' && !is_arg)
        {
            if (item != "")
            {
                VERB3 log(" adding (%s,%s)", item.c_str(), "");
                pluginsWithArgs.push_back(make_pair(item, ""));
                item = "";
            }
            continue;
        }
        item += pValue[ii];
    }
    if (item != "")
    {
        VERB3 log(" adding (%s,%s)", item.c_str(), "");
        pluginsWithArgs.push_back(make_pair(item, ""));
    }
    return pluginsWithArgs;
}

static void ParseCommon()
{
    map_string_t::const_iterator end = s_mapSectionCommon.end();
    map_string_t::const_iterator it = s_mapSectionCommon.find("OpenGPGCheck");
    if (it != end)
    {
        g_settings_bOpenGPGCheck = string_to_bool(it->second.c_str());
    }
    it = s_mapSectionCommon.find("BlackList");
    if (it != end)
    {
        g_settings_mapBlackList = ParseList(it->second.c_str());
    }
    it = s_mapSectionCommon.find("Database");
    if (it != end)
    {
        g_settings_sDatabase = it->second;
    }
    it = s_mapSectionCommon.find("MaxCrashReportsSize");
    if (it != end)
    {
        g_settings_nMaxCrashReportsSize = xatoi_u(it->second.c_str());
    }
    it = s_mapSectionCommon.find("ActionsAndReporters");
    if (it != end)
    {
        g_settings_vectorActionsAndReporters = ParseListWithArgs(it->second.c_str());
    }
    it = s_mapSectionCommon.find("ProcessUnpackaged");
    if (it != end)
    {
        g_settings_bProcessUnpackaged = string_to_bool(it->second.c_str());
    }
}

static void ParseCron()
{
    map_string_t::iterator it = s_mapSectionCron.begin();
    for (; it != s_mapSectionCron.end(); it++)
    {
        vector_pair_string_string_t actionsAndReporters = ParseListWithArgs(it->second.c_str());
        g_settings_mapCron[it->first] = actionsAndReporters;
    }
}

static set_string_t ParseKey(const char *Key)
{
    unsigned int ii;
    std::string item;
    std::string key;
    set_string_t set;
    bool is_quote = false;
    for (ii = 0; Key[ii]; ii++)
    {
        if (Key[ii] == '\"')
        {
            is_quote = !is_quote;
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
    map_string_t::iterator it = s_mapSectionAnalyzerActionsAndReporters.begin();
    for (; it != s_mapSectionAnalyzerActionsAndReporters.end(); it++)
    {
        set_string_t keys = ParseKey(it->first.c_str());
        vector_pair_string_string_t actionsAndReporters = ParseListWithArgs(it->second.c_str());
        set_string_t::iterator it_keys = keys.begin();
        for (; it_keys != keys.end(); it_keys++)
        {
            VERB2 log("AnalyzerActionsAndReporters['%s']=...", it_keys->c_str());
            g_settings_mapAnalyzerActionsAndReporters[*it_keys] = actionsAndReporters;
        }
    }
}

static void LoadGPGKeys()
{
    FILE *fp = fopen(CONF_DIR"/gpg_keys", "r");
    if (fp)
    {
        /* every line is one key
         * FIXME: make it more robust, it doesn't handle comments
         */
        char line[512];
        while (fgets(line, sizeof(line), fp))
        {
            if (line[0] == '/') // probably the begining of path, so let's handle it as a key
            {
                strchrnul(line, '\n')[0] = '\0';
                g_settings_setOpenGPGPublicKeys.insert(line);
            }
        }
        fclose(fp);
    }
}

/* abrt daemon loads .conf file */
void LoadSettings()
{
    FILE *fp = fopen(CONF_DIR"/abrt.conf", "r");
    if (fp)
    {
        char line[512];
        std::string section;
        while (fgets(line, sizeof(line), fp))
        {
            strchrnul(line, '\n')[0] = '\0';
            unsigned ii;
            bool is_key = true;
            bool is_section = false;
            bool is_quote = false;
            std::string key;
            std::string value;
            for (ii = 0; line[ii] != '\0'; ii++)
            {
                if (is_quote && line[ii] == '\\' && line[ii+1] != '\0')
                {
                    value += line[ii];
                    ii++;
                    value += line[ii];
                    continue;
                }
                if (isspace(line[ii]) && !is_quote)
                {
                    continue;
                }
                if (line[ii] == '#' && !is_quote && key == "")
                {
                    break;
                }
                if (line[ii] == '[' && !is_quote)
                {
                    is_section = true;
                    section = "";
                    continue;
                }
                if (line[ii] == '"')
                {
                    is_quote = !is_quote;
                    value += line[ii];
                    continue;
                }
                if (is_section)
                {
                    if (line[ii] == ']')
                    {
                        break;
                    }
                    section += line[ii];
                    continue;
                }
                if (line[ii] == '=' && !is_quote)
                {
                    is_key = false;
                    key = value;
                    value = "";
                    continue;
                }
                value += line[ii];
            }
            if (!is_key && !is_quote)
            {
                if (section == SECTION_COMMON)
                {
                    if (s_mapSectionCommon[key] != "")
                    {
                        s_mapSectionCommon[key] += ",";
                    }
                    s_mapSectionCommon[key] += value;
                }
                else if (section == SECTION_ANALYZER_ACTIONS_AND_REPORTERS)
                {
                    if (s_mapSectionAnalyzerActionsAndReporters[key] != "")
                    {
                        s_mapSectionAnalyzerActionsAndReporters[key] += ",";
                    }
                    s_mapSectionAnalyzerActionsAndReporters[key] += value;
                }
                else if (section == SECTION_CRON)
                {
                    if (s_mapSectionCron[key] != "")
                    {
                        s_mapSectionCron[key] += ",";
                    }
                    s_mapSectionCron[key] += value;
                }
            }
        }
        fclose(fp);
    }
    ParseCommon();
    ParseAnalyzerActionsAndReporters();
    ParseCron();

    /*
        loading gpg keys will invoke LoadOpenGPGPublicKey() from rpm.cpp
        pgpReadPkts which makes nss to re-init and thus makes
        bugzilla plugin work :-/
    */

    //FIXME FIXME FIXME FIXME FIXME FIXME!!!
    //if(g_settings_bOpenGPGCheck)
    LoadGPGKeys();
}

/* dbus call to retrieve .conf file data from daemon */
map_abrt_settings_t GetSettings()
{
    map_abrt_settings_t ABRTSettings;

    ABRTSettings[SECTION_COMMON] = s_mapSectionCommon;
    ABRTSettings[SECTION_ANALYZER_ACTIONS_AND_REPORTERS] = s_mapSectionAnalyzerActionsAndReporters;
    ABRTSettings[SECTION_CRON] = s_mapSectionCron;

    return ABRTSettings;
}


/*
 * Saving
 */

static void SaveSetString(const char* pKey, const set_string_t& pSet, FILE* pFOut)
{
    fprintf(pFOut, "%s =", pKey);

    const char* fmt = " %s";
    set_string_t::const_iterator it_set = pSet.begin();
    for (; it_set != pSet.end(); it_set++)
    {
        fprintf(pFOut, fmt, it_set->c_str());
        fmt = ",%s";
    }
    fputc('\n', pFOut);
}

static void SaveVectorPairStrings(const char* pKey, const vector_pair_string_string_t& pVector, FILE* pFOut)
{
    fprintf(pFOut, "%s =", pKey);

    const char* fmt = " %s";
    int ii;
    for (ii = 0; ii < pVector.size(); ii++)
    {
        fprintf(pFOut, fmt, pVector[ii].first.c_str());
        if (pVector[ii].second != "")
        {
            fprintf(pFOut, "(%s)", pVector[ii].second.c_str());
        }
        fmt = ",%s";
    }
    fputc('\n', pFOut);
}

static void SaveMapVectorPairStrings(const map_vector_pair_string_string_t& pMap, FILE* pFOut)
{
    map_vector_pair_string_string_t::const_iterator it = pMap.begin();
    for (; it != pMap.end(); it++)
    {
        SaveVectorPairStrings(it->first.c_str(), it->second, pFOut);
    }
    fputc('\n', pFOut);
}

static void SaveSectionHeader(const char* pSection, FILE* pFOut)
{
    fprintf(pFOut, "\n[%s]\n\n", pSection);
}

static void SaveBool(const char* pKey, bool pBool, FILE* pFOut)
{
    fprintf(pFOut, "%s = %s\n", pKey, (pBool ? "yes" : "no"));
}

/* dbus call to change some .conf file data */
void SetSettings(const map_abrt_settings_t& pSettings, const char *dbus_sender)
{
    int polkit_result;

    polkit_result = polkit_check_authorization(dbus_sender,
                       "org.fedoraproject.abrt.change-daemon-settings");
    if (polkit_result != PolkitYes)
    {
        error_msg("user %s not authorized, returned %d", dbus_sender, polkit_result);
        return;
    }
    log("user %s succesfully authorized", dbus_sender);

    map_abrt_settings_t::const_iterator it = pSettings.find(SECTION_COMMON);
    map_abrt_settings_t::const_iterator end = pSettings.end();
    if (it != end)
    {
        s_mapSectionCommon = it->second;
        ParseCommon();
    }
    it = pSettings.find(SECTION_ANALYZER_ACTIONS_AND_REPORTERS);
    if (it != end)
    {
        s_mapSectionAnalyzerActionsAndReporters = it->second;
        ParseAnalyzerActionsAndReporters();
    }
    it = pSettings.find(SECTION_CRON);
    if (it != end)
    {
        s_mapSectionCron = it->second;
        ParseCron();
    }
}
