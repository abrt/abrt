#ifndef SETTINGSFUNC_H_
#define SETTINGSFUNC_H_

#include "Settings.h"
#include <string>
#include <map>

typedef std::map<std::string, std::string> map_settings_t;

void load_settings(const std::string& path, map_settings_t& settings);
void save_settings(const std::string& path, const map_settings_t& settings);

#endif /* SETTINGSFUNC_H_ */
