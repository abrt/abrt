#include <fstream>
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
set_string_t  g_settings_setEnabledPlugins;
std::string   g_settings_sDatabase;
unsigned int  g_settings_nMaxCrashReportsSize = 1000;
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

/* (What format do we parse here?) */
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
            item += pValue[ii];
            ii++;
            item += pValue[ii];
            continue;
        }
        if (pValue[ii] == '"')
        {
            is_quote = !is_quote;
            item += pValue[ii];
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
    map_string_t::const_iterator it = s_mapSectionCommon.find("OpenGPGCheck");
    map_string_t::const_iterator end = s_mapSectionCommon.end();
    if (it != end)
    {
        g_settings_bOpenGPGCheck = string_to_bool(it->second.c_str());
    }
    it = s_mapSectionCommon.find("OpenGPGPublicKeys");
    if (it != end)
    {
        g_settings_setOpenGPGPublicKeys = ParseList(it->second.c_str());
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
    it = s_mapSectionCommon.find("EnabledPlugins");
    if (it != end)
    {
        g_settings_setEnabledPlugins = ParseList(it->second.c_str());
    }
    it = s_mapSectionCommon.find("MaxCrashReportsSize");
    if (it != end)
    {
        g_settings_nMaxCrashReportsSize = atoi(it->second.c_str());
    }
    it = s_mapSectionCommon.find("ActionsAndReporters");
    if (it != end)
    {
        g_settings_vectorActionsAndReporters = ParseListWithArgs(it->second.c_str());
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

/* abrt daemon loads .conf file */
void LoadSettings()
{
    std::ifstream fIn;
    fIn.open(CONF_DIR"/abrt.conf");
    if (fIn.is_open())
    {
        std::string line;
        std::string section;
        while (!fIn.eof())
        {
            getline(fIn, line);

            unsigned int ii;
            bool is_key = true;
            bool is_section = false;
            bool is_quote = false;
            std::string key;
            std::string value;
            for (ii = 0; ii < line.length(); ii++)
            {
                if (is_quote && line[ii] == '\\' && ii+1 < line.length())
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
        fIn.close();
    }
    ParseCommon();
    ParseAnalyzerActionsAndReporters();
    ParseCron();
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

/* Rewrite .conf file */
void SaveSettings()
{
    FILE* fOut = fopen(CONF_DIR"/abrt.conf.NEW", "w");

    if (fOut)
    {
        SaveSectionHeader(SECTION_COMMON, fOut);
        SaveBool("OpenGPGCheck", g_settings_bOpenGPGCheck, fOut);
        SaveSetString("OpenGPGPublicKeys", g_settings_setOpenGPGPublicKeys, fOut);
        SaveSetString("BlackList", g_settings_mapBlackList, fOut);
        SaveSetString("EnabledPlugins", g_settings_setEnabledPlugins, fOut);
        fprintf(fOut, "Database = %s\n", g_settings_sDatabase.c_str());
        fprintf(fOut, "MaxCrashReportsSize = %u\n", g_settings_nMaxCrashReportsSize);
        SaveVectorPairStrings("ActionsAndReporters", g_settings_vectorActionsAndReporters, fOut);
        SaveSectionHeader(SECTION_ANALYZER_ACTIONS_AND_REPORTERS, fOut);
        SaveMapVectorPairStrings(g_settings_mapAnalyzerActionsAndReporters, fOut);
        SaveSectionHeader(SECTION_CRON, fOut);
        SaveMapVectorPairStrings(g_settings_mapCron, fOut);
        if (fclose(fOut) == 0 && rename(CONF_DIR"/abrt.conf.NEW", CONF_DIR"/abrt.conf") == 0)
        {
            return; /* success */
        }
    }
    perror_msg("Error saving '%s'", CONF_DIR"/abrt.conf");
    if (fOut)
        unlink(CONF_DIR"/abrt.conf.NEW");
}

/* dbus call to change some .conf file data */
void SetSettings(const map_abrt_settings_t& pSettings, const char *dbus_sender)
{
    bool dirty = false;
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
        dirty = true;
    }
    it = pSettings.find(SECTION_ANALYZER_ACTIONS_AND_REPORTERS);
    if (it != end)
    {
        s_mapSectionAnalyzerActionsAndReporters = it->second;
        ParseAnalyzerActionsAndReporters();
        dirty = true;
    }
    it = pSettings.find(SECTION_CRON);
    if (it != end)
    {
        s_mapSectionCron = it->second;
        ParseCron();
        dirty = true;
    }
    if (dirty)
    {
        SaveSettings();
    }
}
