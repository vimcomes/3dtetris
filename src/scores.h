#pragma once

#include <string>

int load_high_score(const std::string& path);
void save_high_score(const std::string& path, int score);
