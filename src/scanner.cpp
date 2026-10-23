#include "scanner.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <glib.h>

namespace fs = std::filesystem;

namespace {

std::string lower(const std::string& value) {
    gchar* lowered = g_utf8_strdown(value.c_str(), -1);
    std::string result = lowered ? lowered : value;
    g_free(lowered);
    return result;
}

std::string capitalize(std::string value) {
    std::replace(value.begin(), value.end(), '_', ' ');
    bool beginning = true;
    for (char& ch : value) {
        if (beginning && std::isalpha(static_cast<unsigned char>(ch))) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            beginning = false;
        } else if (ch == ' ' || ch == '-' || ch == ':') {
            beginning = true;
        } else if (std::isalpha(static_cast<unsigned char>(ch))) {
            beginning = false;
        }
    }
    return value;
}

bool hasExtension(const fs::path& path, const std::vector<std::string>& extensions) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
}

std::string inferSystem(const std::string& path) {
    const std::string normalized = lower(path);
    auto contains = [&](const char* needle) {
        return normalized.find(needle) != std::string::npos;
    };
    if (contains("cps1") || contains("capcom play system 1")) {
        return "CPS1";
    }
    if (contains("cps2") || contains("capcom play system 2")) {
        return "CPS2";
    }
    if (contains("cps3") || contains("capcom play system 3")) {
        return "CPS3";
    }
    if (contains("neogeo pocket") || contains("neo geo pocket")) {
        return "Neo Geo Pocket";
    }
    if (contains("neogeo") || contains("neo geo") || contains("neorage")) {
        return "Neo Geo";
    }
    if (contains("zx spectrum") || contains("spectrum")) {
        return "ZX Spectrum";
    }
    if (contains("super nintendo") || contains("snes")) {
        return "SNES";
    }
    if (contains("nintendo entertainment") || contains("/nes/") || contains("nes/")) {
        return "NES";
    }
    if (contains("famicom disk") || contains("/fds/")) {
        return "FDS";
    }
    if (contains("mega drive") || contains("megadrive") || contains("genesis")) {
        return "Mega Drive";
    }
    if (contains("master system")) {
        return "Master System";
    }
    if (contains("game gear")) {
        return "Game Gear";
    }
    if (contains("supergrafx")) {
        return "SuperGrafx";
    }
    if (contains("turbografx")) {
        return "TurboGrafx-16";
    }
    if (contains("pc engine")) {
        return "PC Engine";
    }
    if (contains("colecovision")) {
        return "ColecoVision";
    }
    if (contains("/msx/") || contains("msx")) {
        return "MSX";
    }
    return "Arcade";
}

bool isBiosArchive(const std::string& rom) {
    static const std::vector<std::string> bios = {
        "neogeo", "neocd", "neocdz", "neogeo-cd", "pgm", "skns", "cps3boot",
    };
    return std::find(bios.begin(), bios.end(), rom) != bios.end();
}

} // namespace

std::vector<Game> Scanner::scan(const Config& config, const Database& database) const {
    std::vector<Game> games;
    std::unordered_map<std::string, size_t> byRom;
    const std::vector<std::string> extensions = {".zip", ".7z"};

    for (const std::string& directory : config.romDirs) {
        std::error_code error;
        if (!fs::is_directory(directory, error)) {
            continue;
        }

        fs::recursive_directory_iterator iterator(
            directory, fs::directory_options::skip_permission_denied, error);
        const fs::recursive_directory_iterator end;

        while (!error && iterator != end) {
            const fs::directory_entry& entry = *iterator;
            const std::string filename = entry.path().filename().string();

            if (!filename.empty() && filename.front() == '.') {
                iterator.increment(error);
                continue;
            }

            if (entry.is_regular_file(error) && hasExtension(entry.path(), extensions)) {
                std::string rom = entry.path().stem().string();
                const std::string romKey = lower(rom);
                if (!rom.empty() && !isBiosArchive(romKey)) {
                    const GameInfo* info = database.findForPath(romKey, entry.path().string());
                    Game game;
                    game.rom = romKey;
                    game.path = entry.path().string();
                    game.favorite = config.isFavorite(romKey);

                    if (info) {
                        game.title = info->title;
                        game.system = info->system;
                        game.year = info->year;
                        game.manufacturer = info->manufacturer;
                    } else {
                        game.title = capitalize(rom);
                        game.system = inferSystem(game.path);
                    }

                    if (game.system.empty()) {
                        game.system = "Arcade";
                    }
                    if (game.title.empty()) {
                        game.title = capitalize(rom);
                    }

                    game.search = lower(game.title + " " + game.rom + " " + game.system + " " +
                                        game.year + " " + game.manufacturer);

                    const auto existing = byRom.find(romKey);
                    if (existing == byRom.end()) {
                        byRom[romKey] = games.size();
                        games.push_back(std::move(game));
                    } else if (games[existing->second].path.size() > game.path.size()) {
                        games[existing->second] = std::move(game);
                    }
                }
            }

            iterator.increment(error);
        }
    }

    std::sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        const std::string aTitle = lower(a.title);
        const std::string bTitle = lower(b.title);
        if (aTitle != bTitle) {
            return aTitle < bTitle;
        }
        return a.rom < b.rom;
    });

    return games;
}
