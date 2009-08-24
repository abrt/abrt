#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <string>
#include <map>
#include <set>
#include <vector>

#include "MiddleWareTypes.h"

class CSettings
{
    public:
        typedef std::map<std::string, std::string> map_settings_t;
        typedef std::set<std::string> set_strings_t;
        typedef std::pair<std::string, std::string> pair_string_string_t;
        typedef std::vector<pair_string_string_t> vector_pair_strings_t;
        typedef std::map<std::string, vector_pair_strings_t> map_analyzer_actions_and_reporters_t;
        typedef std::map<std::string, vector_pair_strings_t> map_cron_t;

    private:
        map_settings_t m_mapSettingsCommon;
        map_settings_t m_mapSettingsAnalyzerActionsAndReporters;
        map_settings_t m_mapSettingsCron;

        set_strings_t m_setOpenGPGPublicKeys;
        set_strings_t m_setBlackList;
        set_strings_t m_setEnabledPlugins;
        std::string m_sDatabase;
        vector_pair_string_string_t m_vectorActionsAndReporters;
        map_cron_t m_mapCron;

        bool m_bOpenGPGCheck;
        unsigned int m_nMaxCrashReportsSize;
        map_analyzer_actions_and_reporters_t m_mapAnalyzerActionsAndReporters;

        void ParseCommon();
        void ParseAnalyzerActionsAndReporters();
        void ParseCron();

        set_strings_t ParseList(const std::string& pList);
        vector_pair_strings_t ParseListWithArgs(const std::string& pList);
        set_strings_t ParseKey(const std::string& pKey);

    public:
        CSettings();
        void LoadSettings(const std::string& pPath);
        const set_strings_t& GetBlackList();
        const set_strings_t& GetEnabledPlugins();
        const set_strings_t& GetOpenGPGPublicKeys();
        bool GetOpenGPGCheck();
        const map_analyzer_actions_and_reporters_t& GetAnalyzerActionsAndReporters();
        const unsigned int& GetMaxCrashReportsSize();
        const vector_pair_strings_t& GetActionsAndReporters();
        const std::string& GetDatabase();
        const map_cron_t& GetCron();
};

#endif
