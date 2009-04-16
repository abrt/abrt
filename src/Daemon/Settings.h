#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <string>
#include <map>
#include <set>
#include <vector>

class CSettings
{
    public:
        typedef std::map<std::string, std::string> map_settings_t;
        typedef std::set<std::string> set_strings_t;
        typedef std::pair<std::string, std::string> pair_string_string_t;
        typedef std::map<std::string, set_strings_t> map_analyzer_reporters_t;
        typedef std::set<pair_string_string_t> set_actions_t;
        typedef std::map<std::string, set_actions_t> map_analyzer_actions_t;

    private:
        map_settings_t m_mapSettingsCommon;
        map_settings_t m_mapSettingsReporters;
        map_settings_t m_mapSettingsActions;

        set_strings_t m_setOpenGPGPublicKeys;
        set_strings_t m_setBlackList;
        set_strings_t m_setEnabledPlugins;
        set_strings_t m_setReporters;
        std::string m_sDatabase;
        bool m_bOpenGPGCheck;
        unsigned int m_nMaxCrashReportsSize;
        map_analyzer_reporters_t m_mapAnalyzerReporters;
        map_analyzer_actions_t m_mapAnalyzerActions;

        void ParseCommon();
        void ParseReporters();
        void ParseActions();
        set_strings_t ParseList(const std::string& pList);
        set_strings_t ParseActionKey(const std::string& pKey);
        set_actions_t ParseActionValue(const std::string& pValue);

    public:
        void LoadSettings(const std::string& pPath);
        const set_strings_t& GetBlackList();
        const set_strings_t& GetEnabledPlugins();
        const set_strings_t& GetOpenGPGPublicKeys();
        const bool& GetOpenGPGCheck();
        const map_analyzer_reporters_t& GetAnalyzerReporters();
        const map_analyzer_actions_t& GetAnalyzerActions();
        const unsigned int& GetMaxCrashReportsSize();
        const std::string& GetDatabase();
        const set_strings_t& GetReporters();
};

#endif
