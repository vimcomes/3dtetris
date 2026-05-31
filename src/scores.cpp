#include "scores.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int load_high_score(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return 0;
    int score = 0;
    f >> score;
    return score;
}

void save_high_score(const std::string& path, int score)
{
    std::ofstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Scores: cannot write " << path << "\n";
        return;
    }
    f << score << "\n";
}
