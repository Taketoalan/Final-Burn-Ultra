#include "database.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <glib.h>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }
    size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

std::string decodeXml(std::string value) {
    value = replaceAll(value, "&amp;", "&");
    value = replaceAll(value, "&lt;", "<");
    value = replaceAll(value, "&gt;", ">");
    value = replaceAll(value, "&quot;", "\"");
    value = replaceAll(value, "&apos;", "'");

    size_t position = 0;
    while ((position = value.find("&#", position)) != std::string::npos) {
        const auto semicolon = value.find(';', position);
        if (semicolon == std::string::npos) {
            break;
        }
        const std::string code = value.substr(position + 2, semicolon - position - 2);
        try {
            long number = 0;
            if (!code.empty() && (code[0] == 'x' || code[0] == 'X')) {
                number = std::stol(code.substr(1), nullptr, 16);
            } else {
                number = std::stol(code, nullptr, 10);
            }
            if (number > 0 && number <= 0x10FFFF) {
                gchar encoded[8] = {0};
                const gint length = g_unichar_to_utf8(static_cast<gunichar>(number), encoded);
                if (length > 0) {
                    value.replace(position, semicolon - position + 1, encoded, length);
                    position += length;
                    continue;
                }
            }
        } catch (...) {
        }
        position = semicolon + 1;
    }
    return value;
}

std::string tagValue(const std::string& line, const std::string& tag) {
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    const auto start = line.find(open);
    if (start == std::string::npos) {
        return {};
    }
    const auto end = line.find(close, start + open.size());
    if (end == std::string::npos) {
        return {};
    }
    return decodeXml(trim(line.substr(start + open.size(), end - start - open.size())));
}

std::string attribute(const std::string& line, const std::string& key) {
    const std::string needle = key + "=\"";
    const auto start = line.find(needle);
    if (start == std::string::npos) {
        return {};
    }
    const auto valueStart = start + needle.size();
    const auto end = line.find('"', valueStart);
    if (end == std::string::npos) {
        return {};
    }
    return line.substr(valueStart, end - valueStart);
}

std::string systemFromDat(const std::string& path) {
    std::string name = fs::path(path).stem().string();
    const std::string prefix = "FBNeo_-_";
    if (name.rfind(prefix, 0) == 0) {
        name = name.substr(prefix.size());
    } else if (name.rfind("FBNeo - ", 0) == 0) {
        name = name.substr(9);
    }
    std::replace(name.begin(), name.end(), '_', ' ');

    static const std::unordered_map<std::string, std::string> names = {
        {"neogeo", "Neo Geo"},
        {"nintendo entertainment system", "NES"},
        {"super nintendo entertainment system", "SNES"},
        {"nintendo famicom disk system", "FDS"},
        {"sega master system", "Master System"},
        {"sega game gear", "Game Gear"},
        {"sega sg-1000", "SG-1000"},
        {"megadrive", "Mega Drive"},
        {"pc engine", "PC Engine"},
        {"pc engine supergrafx", "SuperGrafx"},
        {"turbografx-16", "TurboGrafx-16"},
        {"neo geo pocket", "Neo Geo Pocket"},
        {"bally astrocade", "Bally Astrocade"},
        {"fairchild channel f", "Channel F"},
        {"sinclair zx spectrum", "ZX Spectrum"},
        {"arcade", "Arcade"},
    };
    const auto it = names.find(lower(name));
    if (it != names.end()) {
        return it->second;
    }
    return name.empty() ? "Arcade" : name;
}

std::string systemForGame(const std::string& datSystem, const std::string& sourcefile) {
    const std::string source = lower(sourcefile);
    if (contains(source, "cps1")) {
        return "CPS1";
    }
    if (contains(source, "cps2")) {
        return "CPS2";
    }
    if (contains(source, "cps3")) {
        return "CPS3";
    }
    if (contains(source, "neogeo")) {
        return "Neo Geo";
    }
    return datSystem;
}

} // namespace

bool Database::load(const std::string& directory) {
    games_.clear();
    loaded_ = false;

    std::error_code error;
    if (!fs::is_directory(directory, error)) {
        return false;
    }

    std::vector<fs::path> datFiles;
    for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, error)) {
        if (!entry.is_regular_file(error)) {
            continue;
        }
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".dat") {
            datFiles.push_back(entry.path());
        }
    }

    std::sort(datFiles.begin(), datFiles.end());

    for (const fs::path& file : datFiles) {
        std::ifstream input(file);
        if (!input) {
            continue;
        }

        const std::string datSystem = systemFromDat(file.string());
        std::string currentRom;
        std::string currentSource;
        std::string currentTitle;
        std::string currentYear;
        std::string currentManufacturer;
        bool inGame = false;

        std::string line;
        while (std::getline(input, line)) {
            const bool gameStart = line.find("<game name=\"") != std::string::npos ||
                                   line.find("<machine name=\"") != std::string::npos;
            if (gameStart) {
                currentRom = attribute(line, "name");
                currentSource = attribute(line, "sourcefile");
                currentTitle.clear();
                currentYear.clear();
                currentManufacturer.clear();
                inGame = !currentRom.empty();
                continue;
            }
            if (!inGame) {
                continue;
            }

            if (currentTitle.empty()) {
                currentTitle = tagValue(line, "description");
            }
            if (currentYear.empty()) {
                currentYear = tagValue(line, "year");
            }
            if (currentManufacturer.empty()) {
                currentManufacturer = tagValue(line, "manufacturer");
            }

            if (line.find("</game>") != std::string::npos ||
                line.find("</machine>") != std::string::npos) {
                if (!currentRom.empty()) {
                    GameInfo info;
                    info.title = currentTitle.empty() ? currentRom : currentTitle;
                    info.system = systemForGame(datSystem, currentSource);
                    info.year = currentYear;
                    info.manufacturer = currentManufacturer;
                    info.sourcefile = currentSource;
                    games_[lower(currentRom)].push_back(std::move(info));
                }
                inGame = false;
                currentRom.clear();
            }
        }
    }

    loaded_ = !games_.empty();
    return loaded_;
}

const GameInfo* Database::find(const std::string& rom) const {
    const auto it = games_.find(lower(rom));
    if (it == games_.end() || it->second.empty()) {
        return nullptr;
    }
    return &it->second.front();
}

namespace {

int systemScore(const GameInfo& info, const std::string& path) {
    const std::string system = lower(info.system);
    const std::string location = lower(path);

    auto has = [&](const char* needle) {
        return location.find(needle) != std::string::npos;
    };

    int score = 0;
    if (system == "arcade") {
        score = has("mame") || has("arcade") || has("/fba/") ? 70 : 10;
    } else if (system == "cps1") {
        score = has("cps1") || has("capcom play system 1") ? 95 : 5;
    } else if (system == "cps2") {
        score = has("cps2") || has("capcom play system 2") ? 95 : 5;
    } else if (system == "cps3") {
        score = has("cps3") || has("capcom play system 3") ? 95 : 5;
    } else if (system == "neo geo") {
        if (has("neogeo") || has("neo geo") || has("neorage")) {
            score = 95;
        } else if (has("mame") || has("/fba/")) {
            score = 40;
        }
    } else {
        if (!system.empty() && has(system.c_str())) {
            score = 100;
        }
    }
    return score;
}

} // namespace

const GameInfo* Database::findForPath(const std::string& rom, const std::string& path) const {
    const auto it = games_.find(lower(rom));
    if (it == games_.end() || it->second.empty()) {
        return nullptr;
    }

    const GameInfo* best = &it->second.front();
    int bestScore = systemScore(*best, path);
    for (const GameInfo& candidate : it->second) {
        const int score = systemScore(candidate, path);
        if (score > bestScore) {
            best = &candidate;
            bestScore = score;
        }
    }
    return best;
}
