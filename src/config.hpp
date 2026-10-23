#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class Config {
public:
    Config();

    void load();
    void save() const;
    void ensureDefaults();
    void syncRomPathsToFbneoIni() const;

    bool isFavorite(const std::string& rom) const;
    void toggleFavorite(const std::string& rom);

    std::string fbneoIniPath() const;
    std::vector<std::string> artworkDirs() const;

    std::string emulatorPath;
    std::vector<std::string> romDirs;
    std::string datDir;
    std::string artworkDir;
    std::string extraArgs;
    bool fullscreen = false;
    bool syncRomPaths = true;
    bool autoDat = true;
    std::unordered_set<std::string> favorites;

    static std::string configDir();
    static std::string configFile();
    static std::string userDataDir();
    static std::string findEmulator();
    static std::vector<std::string> detectRomDirs();
    static std::string detectDataDir();
    static std::string detectDatDir();
    static std::string expandPath(const std::string& path);
};
