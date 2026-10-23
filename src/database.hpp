#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct GameInfo {
    std::string title;
    std::string system;
    std::string year;
    std::string manufacturer;
    std::string sourcefile;
};

class Database {
public:
    bool load(const std::string& directory);
    const GameInfo* find(const std::string& rom) const;
    const GameInfo* findForPath(const std::string& rom, const std::string& path) const;
    size_t size() const { return games_.size(); }
    bool loaded() const { return loaded_; }

private:
    std::unordered_map<std::string, std::vector<GameInfo>> games_;
    bool loaded_ = false;
};
