#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "abrt_types.h"

typedef map_vector_pair_string_string_t map_analyzer_actions_and_reporters_t;
typedef map_vector_pair_string_string_t map_cron_t;

void LoadSettings(const char* pPath);
void SaveSettings(const char* pPath);

extern set_strings_t g_settings_setOpenGPGPublicKeys;
extern set_strings_t g_settings_mapSettingsBlackList;
extern set_strings_t g_settings_setEnabledPlugins;
extern unsigned int  g_settings_nMaxCrashReportsSize;
extern bool          g_settings_bOpenGPGCheck;
extern std::string   g_settings_sDatabase;
extern map_cron_t    g_settings_mapCron;
extern vector_pair_string_string_t g_settings_vectorActionsAndReporters;
extern map_analyzer_actions_and_reporters_t g_settings_mapAnalyzerActionsAndReporters;

#endif
