#pragma once

#include "config.hpp"
#include "database.hpp"

#include <string>
#include <vector>

struct Game {
    std::string rom;
    std::string title;
    std::string system;
    std::string year;
    std::string manufacturer;
    std::string path;
    bool favorite = false;
    std::string search;
};

class Scanner {
public:
    std::vector<Game> scan(const Config& config, const Database& database) const;
};
